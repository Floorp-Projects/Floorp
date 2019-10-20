// |jit-test| error:unsafe filename: (invalid UTF-8 filename)
setTestFilenameValidationCallback();
evaluate("throw 2", {fileName: "\uDEFF"});
