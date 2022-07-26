/* eslint-env mozilla/chrome-script */

Cu.importGlobalProperties(["File"]);

addMessageListener("create-file-objects", function(message) {
  let files = [];
  let promises = [];
  for (fileName of message.fileNames) {
    promises.push(
      File.createFromFileName(fileName).then(function(file) {
        files.push(file);
      })
    );
  }

  Promise.all(promises).then(function() {
    sendAsyncMessage("created-file-objects", files);
  });
});
