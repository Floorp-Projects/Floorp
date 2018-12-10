// |jit-test| error: ReferenceError: WritableStream is not defined

load(libdir + "web-platform-testharness.js");
load_web_platform_test("streams/readable-streams/reentrant-strategies.js");
