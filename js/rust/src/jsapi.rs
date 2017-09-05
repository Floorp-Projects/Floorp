#[cfg(feature = "debugmozjs")]
include!(concat!(env!("OUT_DIR"), "/jsapi_debug.rs"));

#[cfg(not(feature = "debugmozjs"))]
include!(concat!(env!("OUT_DIR"), "/jsapi.rs"));
