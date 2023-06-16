// |jit-test| error:unsafe filename: 
setTestFilenameValidationCallback();
evaluate("throw 2", {fileName: "\uDEFF"});
