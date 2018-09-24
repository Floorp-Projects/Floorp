extern crate ipdl;

use std::collections::HashSet;
use std::ffi::OsStr;
use std::fs;
use std::fs::File;
use std::io::Read;
use std::path::PathBuf;

const BASE_PATH: [&str; 5] = ["..", "..", "ipdl", "test", "ipdl"];
const OK_PATH: &str = "ok";
const ERROR_PATH: &str = "error";

// Tests in error/ are disabled because the given checking is not
// enabled yet.

const DISABLED_TESTS: &[&str] = &["unknownSyncMessage.ipdl", "unknownIntrMessage.ipdl", "asyncMessageListed.ipdl"];

// XXX This does not run efficiently. If A includes B, then we end up
// testing A and B two times each. At least for the non-error case we
// should be able to do them all together.

fn test_files(test_file_path: &str, should_pass: bool) {
    let mut path: PathBuf = BASE_PATH.iter().collect();
    path.push(test_file_path);

    let include_dirs = vec![path.clone()];

    let mut disabled_tests = HashSet::new();
    for f in DISABLED_TESTS {
        disabled_tests.insert(OsStr::new(f));
    }

    let entries = fs::read_dir(&path).expect("Should have the test file directory");
    for entry in entries {
        if let Ok(entry) = entry {
            let expected_result = if !should_pass
                && disabled_tests.contains(
                    entry
                        .path()
                        .file_name()
                        .expect("No filename for path in test directory"),
                ) {
                println!(
                    "Expecting test to pass when it should fail {:?}",
                    entry.file_name()
                );
                true
            } else {
                println!("Testing {:?}", entry.file_name());
                should_pass
            };

            let file_name = entry.path();
            let ok = ipdl::parser::parse(&file_name, &include_dirs, |src| {
                let mut buffer = Vec::new();
                let mut file = File::open(src).map_err(|_| ())?;
                file.read_to_end(&mut buffer).map_err(|_| ())?;
                Ok(Box::new(buffer))
            }).is_ok();
            assert!(expected_result == ok);
        }
    }
}

#[test]
fn ok_tests() {
    test_files(OK_PATH, true);
}

#[test]
fn error_tests() {
    test_files(ERROR_PATH, false);
}
