function finish() {
  postMessage({ type: 'finish' });
}

function ok(a, msg) {
  postMessage({ type: 'test', test: !!a, message: msg });
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function isnot(a, b, msg) {
  ok(a != b, msg);
}

function checkSubDir(dir) {
  return dir.getFilesAndDirectories().then(
    function(data) {
      for (var i = 0; i < data.length; ++i) {
        ok (data[i] instanceof File || data[i] instanceof Directory, "Just Files or Directories");
        if (data[i] instanceof Directory) {
          isnot(data[i].name, '/', "Subdirectory should be called with the leafname");
          isnot(data[i].path, '/', "Subdirectory path should be called with the leafname");
          isnot(data[i].path, dir.path, "Subdirectory path should contain the parent path.");
          is(data[i].path,dir.path + '/' + data[i].name, "Subdirectory path should be called parentdir.path + '/' + leafname");
        }
      }
    }
  );
}

onmessage = function(e) {
  var directory = e.data;
  ok(directory instanceof Directory, "This is a directory.");

  directory.getFilesAndDirectories().then(
    function(data) {
      ok(data.length, "We should have some data.");
      var promises = [];
      for (var i = 0; i < data.length; ++i) {
        ok (data[i] instanceof File || data[i] instanceof Directory, "Just Files or Directories");
        if (data[i] instanceof Directory) {
          isnot(data[i].name, '/', "Subdirectory should be called with the leafname");
          is(data[i].path, '/' + data[i].name, "Subdirectory path should be called '/' + leafname");
          promises.push(checkSubDir(data[i]));
        }
      }

      return Promise.all(promises);
    },
    function() {
      ok(false, "Something when wrong");
    }
  ).then(finish);
}
