/* eslint-env mozilla/chrome-script */

Cu.importGlobalProperties(["File"]);

addMessageListener("files.open", function(message) {
  let list = [];
  let promises = [];
  for (let path of message) {
    promises.push(
      File.createFromFileName(path).then(file => {
        list.push(file);
      })
    );
  }

  Promise.all(promises).then(() => {
    sendAsyncMessage("files.opened", list);
  });
});
