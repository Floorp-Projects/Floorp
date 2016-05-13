function setup_tests(aNext) {
  SpecialPowers.pushPrefEnv({"set": [["dom.input.dirpicker", true],
                                     ["dom.webkitBlink.dirPicker.enabled", true]]}, aNext);
}

function test_basic(aDirectory, aNext) {
  ok(aDirectory, "Directory exists.");
  ok(aDirectory instanceof Directory, "We have a directory.");
  is(aDirectory.path, '/' + aDirectory.name, "directory.path must be '/'+name");
  aNext();
}

function test_getFilesAndDirectories(aDirectory, aRecursive, aNext) {
  function checkSubDir(dir) {
    return dir.getFilesAndDirectories().then(
      function(data) {
        for (var i = 0; i < data.length; ++i) {
          ok (data[i] instanceof File || data[i] instanceof Directory, "Just Files or Directories");
          if (data[i] instanceof Directory) {
            isnot(data[i].name, '/', "Subdirectory should be called with the leafname");
            isnot(data[i].path, '/', "Subdirectory path should be called with the leafname");
            isnot(data[i].path, dir.path, "Subdirectory path should contain the parent path.");
            is(data[i].path, dir.path + '/' + data[i].name, "Subdirectory path should be called parentdir.path + '/' + leafname");
          }

          if (data[i] instanceof File) {
            is(data[i].webkitRelativePath, dir.path + '/' + data[i].name, "File.webkitRelativePath should be called: parentdir.path + '/' + file.name");
          }
        }
      }
    );
  }

  aDirectory.getFilesAndDirectories().then(
    function(data) {
      ok(data.length, "We should have some data.");
      var promises = [];
      for (var i = 0; i < data.length; ++i) {
        ok (data[i] instanceof File || data[i] instanceof Directory, "Just Files or Directories: " + data[i].name);
        if (data[i] instanceof Directory) {
          isnot(data[i].name, '/', "Subdirectory should be called with the leafname");
          is(data[i].path, aDirectory.path + '/' + data[i].name, "Subdirectory path should be called parentdir.path + '/' + leafname");
          if (aRecursive) {
            promises.push(checkSubDir(data[i]));
          }
        }

        if (data[i] instanceof File) {
          is(data[i].webkitRelativePath, aDirectory.path + '/' + data[i].name, "File.webkitRelativePath should be called '/' + file.name");
        }
      }

      return Promise.all(promises);
    },
    function() {
      ok(false, "Something when wrong");
    }
  ).then(aNext);
}

function test_getFiles(aDirectory, aRecursive, aNext) {
  aDirectory.getFiles(aRecursive).then(
    function(data) {
      for (var i = 0; i < data.length; ++i) {
        ok(data[i] instanceof File, "File: " + data[i].name);
        ok(data[i].webkitRelativePath.indexOf(aDirectory.path) == 0 &&
           data[i].webkitRelativePath.indexOf('/' + data[i].name) + ('/' + data[i].name).length == data[i].webkitRelativePath.length,
           "File.webkitRelativePath should be called dir.path + '/' + file.name: " + data[i].webkitRelativePath);
      }
    },
    function() {
      ok(false, "Something when wrong");
    }
  ).then(aNext);
}

function test_getFiles_recursiveComparison(aDirectory, aNext) {
  aDirectory.getFiles(true).then(function(data) {
    is(data.length, 2, "Only 2 files for this test.");
    ok(data[0].name == 'foo.txt' || data[0].name == 'bar.txt', "First filename matches");
    ok(data[1].name == 'foo.txt' || data[1].name == 'bar.txt', "Second filename matches");
  }).then(function() {
    return aDirectory.getFiles(false);
  }).then(function(data) {
    is(data.length, 1, "Only 1 file for this test.");
    ok(data[0].name == 'foo.txt' || data[0].name == 'bar.txt', "First filename matches");
  }).catch(function() {
    ok(false, "Something when wrong");
  }).then(aNext);
}
