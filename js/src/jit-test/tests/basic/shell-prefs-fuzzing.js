// |jit-test| --fuzzing-safe; --setpref=foobar=123; error:987
// If --fuzzing-safe is used, unknown pref names are ignored, similar to unknown
// shell flags.
throw 987;
