/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

mod commands;
use commands::ResultCode;
use std::io::Error;
use std::io::ErrorKind;

fn main() -> Result<(), Error> {
    // The general structure of these functions is to print error cases to
    // stdout so that the extension can read them and then do error-handling
    // on that end.
    let message_length: u32 =
        commands::read_message_length(std::io::stdin()).or_else(|_| -> Result<u32, _> {
            commands::generate_response("Failed to read message length", ResultCode::Error.into())
                .expect("JSON error");
            return Err(Error::new(
                ErrorKind::InvalidInput,
                "Failed to read message length",
            ));
        })?;
    let message: String = commands::read_message_string(std::io::stdin(), message_length).or_else(
        |_| -> Result<String, _> {
            commands::generate_response("Failed to read message", ResultCode::Error.into())
                .expect("JSON error");
            return Err(Error::new(
                ErrorKind::InvalidInput,
                "Failed to read message",
            ));
        },
    )?;
    // Deserialize the message with the following expected format
    let native_messaging_json: commands::FirefoxCommand =
        serde_json::from_str(&message).or_else(|_| -> Result<commands::FirefoxCommand, _> {
            commands::generate_response(
                "Failed to deserialize message JSON",
                ResultCode::Error.into(),
            )
            .expect("JSON error");
            return Err(Error::new(
                ErrorKind::InvalidInput,
                "Failed to deserialize message JSON",
            ));
        })?;
    commands::process_command(&native_messaging_json).or_else(|e| -> Result<bool, _> {
        commands::generate_response(
            format!("Failed to process command: {}", e).as_str(),
            ResultCode::Error.into(),
        )
        .expect("JSON error");
        return Err(Error::new(
            ErrorKind::InvalidInput,
            "Failed to process command",
        ));
    })?;
    Ok(())
}
