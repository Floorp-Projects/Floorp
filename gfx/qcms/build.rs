use rustc_version::{version, Version};

fn main() {
    if cfg!(feature = "neon") && std::env::var_os("RUSTC_BOOTSTRAP").is_none() {
        // With Rust 1.50 and newer RUSTC_BOOTSTAP should
        // be set externally so we only set it if it's
        // not already set.
        println!("cargo:rustc-env=RUSTC_BOOTSTRAP=1");
    }
    let target = std::env::var("TARGET").expect("TARGET environment variable not defined");
    if target.contains("neon") {
        println!("cargo:rustc-cfg=libcore_neon");
    }
    if version().unwrap() >= Version::parse("1.60.0-alpha").unwrap() {
        println!("cargo:rustc-cfg=std_arch");
    }
    if version().unwrap() < Version::parse("1.61.0-alpha").unwrap() {
        println!("cargo:rustc-cfg=aarch64_target_feature");
    }
}
