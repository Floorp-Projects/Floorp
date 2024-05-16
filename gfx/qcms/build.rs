extern crate version_check as rustc;

fn main() {
    if rustc::is_min_version("1.80.0").unwrap_or(false) {
        println!("cargo::rustc-check-cfg=cfg(stdsimd_split)");
    }
    if rustc::is_min_version("1.78.0").unwrap_or(false) {
        println!("cargo::rustc-cfg=stdsimd_split");
    }
}
