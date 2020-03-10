#[no_mangle]
pub extern "C" fn test_rust() -> *const u8 {
    // NB: rust &str aren't null terminated.
    let greeting = "hello from rust.\0";
    greeting.as_ptr()
}
