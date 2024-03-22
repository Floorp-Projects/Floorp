/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde::{Deserialize, Serialize};
use std::io::{self, Read, Write};
use std::path::PathBuf;
use std::process::Command;
use url::Url;

#[cfg(target_os = "windows")]
const OS_NAME: &str = "windows";

#[cfg(target_os = "macos")]
const OS_NAME: &str = "macos";

#[derive(Serialize, Deserialize)]
#[serde(tag = "command", content = "data")]
// {
//     "command": "LaunchFirefox",
//     "data": {"url": "https://example.com"},
// }
pub enum FirefoxCommand {
    LaunchFirefox { url: String },
    LaunchFirefoxPrivate { url: String },
    GetVersion {},
    GetInstallId {},
}
#[derive(Serialize, Deserialize)]
// {
//     "message": "Successful launch",
//     "result_code": 1,
// }
pub struct Response {
    pub message: String,
    pub result_code: u32,
}

#[derive(Serialize, Deserialize)]
// {
//     "installation_id": "123ABC456",
// }
pub struct InstallationId {
    pub installation_id: String,
}

#[repr(u32)]
pub enum ResultCode {
    Success = 0,
    Error = 1,
}
impl From<ResultCode> for u32 {
    fn from(m: ResultCode) -> u32 {
        m as u32
    }
}

trait CommandRunner {
    fn new() -> Self
    where
        Self: Sized;
    fn arg(&mut self, arg: &str) -> &mut Self;
    fn args(&mut self, args: &[&str]) -> &mut Self;
    fn spawn(&mut self) -> std::io::Result<()>;
    fn to_string(&mut self) -> std::io::Result<String>;
}

impl CommandRunner for Command {
    fn new() -> Self {
        #[cfg(target_os = "macos")]
        {
            Command::new("open")
        }
        #[cfg(target_os = "windows")]
        {
            use mozbuild::config::MOZ_APP_NAME;
            use std::env;
            use std::path::Path;
            // Get the current executable's path, we know Firefox is in the
            // same folder is nmhproxy.exe so we can use that.
            let nmh_exe_path = env::current_exe().unwrap();
            let nmh_exe_folder = nmh_exe_path.parent().unwrap_or_else(|| Path::new(""));
            let moz_exe_path = nmh_exe_folder.join(format!("{}.exe", MOZ_APP_NAME));
            Command::new(moz_exe_path)
        }
    }
    fn arg(&mut self, arg: &str) -> &mut Self {
        self.arg(arg)
    }
    fn args(&mut self, args: &[&str]) -> &mut Self {
        self.args(args)
    }
    fn spawn(&mut self) -> std::io::Result<()> {
        self.spawn().map(|_| ())
    }
    fn to_string(&mut self) -> std::io::Result<String> {
        Ok("".to_string())
    }
}

struct MockCommand {
    command_line: String,
}

impl CommandRunner for MockCommand {
    fn new() -> Self {
        MockCommand {
            command_line: String::new(),
        }
    }
    fn arg(&mut self, arg: &str) -> &mut Self {
        self.command_line.push_str(arg);
        self.command_line.push(' ');
        self
    }
    fn args(&mut self, args: &[&str]) -> &mut Self {
        for arg in args {
            self.command_line.push_str(arg);
            self.command_line.push(' ');
        }
        self
    }
    fn spawn(&mut self) -> std::io::Result<()> {
        Ok(())
    }
    fn to_string(&mut self) -> std::io::Result<String> {
        Ok(self.command_line.clone())
    }
}

// The message length is a 32-bit integer in native byte order
pub fn read_message_length<R: Read>(mut reader: R) -> std::io::Result<u32> {
    let mut buffer = [0u8; 4];
    reader.read_exact(&mut buffer)?;
    let length: u32 = u32::from_ne_bytes(buffer);
    if (length > 0) && (length < 100 * 1024) {
        Ok(length)
    } else {
        Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Invalid message length",
        ))
    }
}

pub fn read_message_string<R: Read>(mut reader: R, length: u32) -> io::Result<String> {
    let mut buffer = vec![0u8; length.try_into().unwrap()];
    reader.read_exact(&mut buffer)?;
    let message =
        String::from_utf8(buffer).map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))?;
    Ok(message)
}

pub fn process_command(command: &FirefoxCommand) -> std::io::Result<bool> {
    match &command {
        FirefoxCommand::LaunchFirefox { url } => {
            launch_firefox::<Command>(url.to_owned(), false, OS_NAME)?;
            Ok(true)
        }
        FirefoxCommand::LaunchFirefoxPrivate { url } => {
            launch_firefox::<Command>(url.to_owned(), true, OS_NAME)?;
            Ok(true)
        }
        FirefoxCommand::GetVersion {} => generate_response("1", ResultCode::Success.into()),
        FirefoxCommand::GetInstallId {} => {
            // config_dir() evaluates to ~/Library/Application Support on macOS
            // and %RoamingAppData% on Windows.
            let mut json_path = match dirs::config_dir() {
                Some(path) => path,
                None => {
                    return generate_response(
                        "Config dir could not be found",
                        ResultCode::Error.into(),
                    )
                }
            };
            #[cfg(target_os = "windows")]
            json_path.push("Mozilla\\Firefox");
            #[cfg(target_os = "macos")]
            json_path.push("Firefox");

            json_path.push("install_id");
            json_path.set_extension("json");
            let mut install_id = String::new();
            get_install_id(&mut json_path, &mut install_id)
        }
    }
}

pub fn generate_response(message: &str, result_code: u32) -> std::io::Result<bool> {
    let response_struct = Response {
        message: message.to_string(),
        result_code,
    };
    let response_str = serde_json::to_string(&response_struct)
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e))?;
    let response_len_bytes: [u8; 4] = (response_str.len() as u32).to_ne_bytes();
    std::io::stdout().write_all(&response_len_bytes)?;
    std::io::stdout().write_all(response_str.as_bytes())?;
    std::io::stdout().flush()?;
    Ok(true)
}

fn validate_url(url: String) -> std::io::Result<String> {
    let parsed_url = Url::parse(url.as_str())
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e))?;
    match parsed_url.scheme() {
        "http" | "https" | "file" => Ok(parsed_url.to_string()),
        _ => Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "Invalid URL scheme",
        )),
    }
}

fn launch_firefox<C: CommandRunner>(
    url: String,
    private: bool,
    os: &str,
) -> std::io::Result<String> {
    let validated_url: String = validate_url(url)?;
    let mut command = C::new();
    if os == "macos" {
        use mozbuild::config::MOZ_MACBUNDLE_ID;
        let mut args: [&str; 2] = ["--args", "-url"];
        if private {
            args[1] = "-private-window";
        }
        command
            .arg("-n")
            .arg("-b")
            .arg(MOZ_MACBUNDLE_ID)
            .args(&args)
            .arg(validated_url.as_str());
    } else if os == "windows" {
        let mut args: [&str; 2] = ["-osint", "-url"];
        if private {
            args[1] = "-private-window";
        }
        command.args(&args).arg(validated_url.as_str());
    }
    match command.spawn() {
        Ok(_) => generate_response(
            if private {
                "Successful private launch"
            } else {
                "Sucessful launch"
            },
            ResultCode::Success.into(),
        )?,
        Err(_) => generate_response(
            if private {
                "Failed private launch"
            } else {
                "Failed launch"
            },
            ResultCode::Error.into(),
        )?,
    };
    command.to_string()
}

fn get_install_id(json_path: &mut PathBuf, install_id: &mut String) -> std::io::Result<bool> {
    if !json_path.exists() {
        return Err(std::io::Error::new(
            std::io::ErrorKind::NotFound,
            "Install ID file does not exist",
        ));
    }
    let json_size = std::fs::metadata(&json_path)
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::NotFound, e))?
        .len();
    // Set a 1 KB limit for the file size.
    if json_size <= 0 || json_size > 1024 {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            "Install ID file has invalid size",
        ));
    }
    let mut file =
        std::fs::File::open(json_path).or_else(|_| -> std::io::Result<std::fs::File> {
            return Err(std::io::Error::new(
                std::io::ErrorKind::NotFound,
                "Failed to open file",
            ));
        })?;
    let mut contents = String::new();
    match file.read_to_string(&mut contents) {
        Ok(_) => match serde_json::from_str::<InstallationId>(&contents) {
            Ok(id) => {
                *install_id = id.installation_id.clone();
                generate_response(&id.installation_id, ResultCode::Success.into())
            }
            Err(_) => {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::InvalidData,
                    "Failed to read installation ID",
                ))
            }
        },
        Err(_) => generate_response("Failed to read file", ResultCode::Error.into()),
    }?;
    Ok(true)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;
    use tempfile::NamedTempFile;
    #[test]
    fn test_validate_url() {
        let valid_test_cases = vec![
            "https://example.com/".to_string(),
            "http://example.com/".to_string(),
            "file:///path/to/file".to_string(),
            "https://test.example.com/".to_string(),
        ];

        for input in valid_test_cases {
            let result = validate_url(input.clone());
            assert!(result.is_ok(), "Expected Ok, got Err");
            // Safe to unwrap because we know the result is Ok
            let ok_value = result.unwrap();
            assert_eq!(ok_value, input);
        }

        assert!(matches!(
            validate_url("fakeprotocol://test.example.com/".to_string()).map_err(|e| e.kind()),
            Err(std::io::ErrorKind::InvalidInput)
        ));

        assert!(matches!(
            validate_url("invalidURL".to_string()).map_err(|e| e.kind()),
            Err(std::io::ErrorKind::InvalidData)
        ));
    }

    #[test]
    fn test_read_message_length_valid() {
        let input: [u8; 4] = 256u32.to_ne_bytes();
        let mut cursor = Cursor::new(input);
        let length = read_message_length(&mut cursor);
        assert!(length.is_ok(), "Expected Ok, got Err");
        assert_eq!(length.unwrap(), 256);
    }

    #[test]
    fn test_read_message_length_invalid_too_large() {
        let input: [u8; 4] = 1_000_000u32.to_ne_bytes();
        let mut cursor = Cursor::new(input);
        let result = read_message_length(&mut cursor);
        assert!(result.is_err());
        let error = result.err().unwrap();
        assert_eq!(error.kind(), io::ErrorKind::InvalidData);
    }

    #[test]
    fn test_read_message_length_invalid_zero() {
        let input: [u8; 4] = 0u32.to_ne_bytes();
        let mut cursor = Cursor::new(input);
        let result = read_message_length(&mut cursor);
        assert!(result.is_err());
        let error = result.err().unwrap();
        assert_eq!(error.kind(), io::ErrorKind::InvalidData);
    }

    #[test]
    fn test_read_message_string_valid() {
        let input_data = b"Valid UTF8 string!";
        let input_length = input_data.len() as u32;
        let message = read_message_string(&input_data[..], input_length);
        assert!(message.is_ok(), "Expected Ok, got Err");
        assert_eq!(message.unwrap(), "Valid UTF8 string!");
    }

    #[test]
    fn test_read_message_string_invalid() {
        let input_data: [u8; 3] = [0xff, 0xfe, 0xfd];
        let input_length = input_data.len() as u32;
        let result = read_message_string(&input_data[..], input_length);
        assert!(result.is_err());
        let error = result.err().unwrap();
        assert_eq!(error.kind(), io::ErrorKind::InvalidData);
    }

    #[test]
    fn test_launch_regular_command_macos() {
        let url = "https://example.com";
        let result = launch_firefox::<MockCommand>(url.to_string(), false, "macos");
        assert!(result.is_ok());
        let command_line = result.unwrap();
        let correct_url_format = format!("-url {}", url);
        assert!(command_line.contains(correct_url_format.as_str()));
    }

    #[test]
    fn test_launch_regular_command_windows() {
        let url = "https://example.com";
        let result = launch_firefox::<MockCommand>(url.to_string(), false, "windows");
        assert!(result.is_ok());
        let command_line = result.unwrap();
        let correct_url_format = format!("-osint -url {}", url);
        assert!(command_line.contains(correct_url_format.as_str()));
    }

    #[test]
    fn test_launch_private_command_macos() {
        let url = "https://example.com";
        let result = launch_firefox::<MockCommand>(url.to_string(), true, "macos");
        assert!(result.is_ok());
        let command_line = result.unwrap();
        let correct_url_format = format!("-private-window {}", url);
        assert!(command_line.contains(correct_url_format.as_str()));
    }

    #[test]
    fn test_launch_private_command_windows() {
        let url = "https://example.com";
        let result = launch_firefox::<MockCommand>(url.to_string(), true, "windows");
        assert!(result.is_ok());
        let command_line = result.unwrap();
        let correct_url_format = format!("-osint -private-window {}", url);
        assert!(command_line.contains(correct_url_format.as_str()));
    }

    #[test]
    fn test_get_install_id_valid() -> std::io::Result<()> {
        let mut tempfile = NamedTempFile::new().unwrap();
        let installation_id = InstallationId {
            installation_id: "123ABC456".to_string(),
        };
        let json_string = serde_json::to_string(&installation_id);
        let _ = tempfile.write_all(json_string?.as_bytes());
        let mut install_id = String::new();
        let result = get_install_id(&mut tempfile.path().to_path_buf(), &mut install_id);
        assert!(result.is_ok());
        assert_eq!(install_id, "123ABC456");
        Ok(())
    }

    #[test]
    fn test_get_install_id_incorrect_var() -> std::io::Result<()> {
        #[derive(Serialize, Deserialize)]
        pub struct IncorrectJSON {
            pub incorrect_var: String,
        }
        let mut tempfile = NamedTempFile::new().unwrap();
        let incorrect_json = IncorrectJSON {
            incorrect_var: "incorrect_val".to_string(),
        };
        let json_string = serde_json::to_string(&incorrect_json);
        let _ = tempfile.write_all(json_string?.as_bytes());
        let mut install_id = String::new();
        let result = get_install_id(&mut tempfile.path().to_path_buf(), &mut install_id);
        assert!(result.is_err());
        let error = result.err().unwrap();
        assert_eq!(error.kind(), std::io::ErrorKind::InvalidData);
        Ok(())
    }

    #[test]
    fn test_get_install_id_partially_correct_vars() -> std::io::Result<()> {
        #[derive(Serialize, Deserialize)]
        pub struct IncorrectJSON {
            pub installation_id: String,
            pub incorrect_var: String,
        }
        let mut tempfile = NamedTempFile::new().unwrap();
        let incorrect_json = IncorrectJSON {
            installation_id: "123ABC456".to_string(),
            incorrect_var: "incorrect_val".to_string(),
        };
        let json_string = serde_json::to_string(&incorrect_json);
        let _ = tempfile.write_all(json_string?.as_bytes());
        let mut install_id = String::new();
        let result = get_install_id(&mut tempfile.path().to_path_buf(), &mut install_id);
        // This still succeeds as the installation_id field is present
        assert!(result.is_ok());
        Ok(())
    }

    #[test]
    fn test_get_install_id_file_does_not_exist() -> std::io::Result<()> {
        let tempfile = NamedTempFile::new().unwrap();
        let mut path = tempfile.path().to_path_buf();
        tempfile.close()?;
        let mut install_id = String::new();
        let result = get_install_id(&mut path, &mut install_id);
        assert!(result.is_err());
        let error = result.err().unwrap();
        assert_eq!(error.kind(), std::io::ErrorKind::NotFound);
        Ok(())
    }

    #[test]
    fn test_get_install_id_file_too_large() -> std::io::Result<()> {
        let mut tempfile = NamedTempFile::new().unwrap();
        let installation_id = InstallationId {
            // Create a ~10 KB file
            installation_id: String::from_utf8(vec![b'X'; 10000]).unwrap(),
        };
        let json_string = serde_json::to_string(&installation_id);
        let _ = tempfile.write_all(json_string?.as_bytes());
        let mut install_id = String::new();
        let result = get_install_id(&mut tempfile.path().to_path_buf(), &mut install_id);
        assert!(result.is_err());
        let error = result.err().unwrap();
        assert_eq!(error.kind(), std::io::ErrorKind::InvalidData);
        Ok(())
    }
}
