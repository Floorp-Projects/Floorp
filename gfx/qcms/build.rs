fn main() {
    if cfg!(feature = "neon") {
        if std::env::var_os("RUSTC_BOOTSTRAP").is_none() {
            // With Rust 1.50 and newer RUSTC_BOOTSTAP should
            // be set externally so we only set it if it's
            // not already set.
            println!("cargo:rustc-env=RUSTC_BOOTSTRAP=1");
        }
    }
    let target = std::env::var("TARGET").expect("TARGET environment variable not defined");
    if target.contains("neon") {
        println!("cargo:rustc-cfg=libcore_neon");
    }
}
