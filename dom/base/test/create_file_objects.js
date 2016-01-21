Components.utils.importGlobalProperties(['File']);

addMessageListener("create-file-objects", function(message) {
  let files = []
  for (fileName of message.fileNames) {
    files.push(new File(fileName));
  }

  sendAsyncMessage("created-file-objects", files);
});
