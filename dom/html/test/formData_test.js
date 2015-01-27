function testHas() {
  var f = new FormData();
  f.append("foo", "bar");
  f.append("another", "value");
  ok(f.has("foo"), "has() on existing name should be true.");
  ok(f.has("another"), "has() on existing name should be true.");
  ok(!f.has("nonexistent"), "has() on non-existent name should be false.");
}

function testGet() {
  var f = new FormData();
  f.append("foo", "bar");
  f.append("foo", "bar2");
  f.append("blob", new Blob(["hey"], { type: 'text/plain' }));
  f.append("file", new File(["hey"], 'testname',  {type: 'text/plain'}));

  is(f.get("foo"), "bar", "get() on existing name should return first value");
  ok(f.get("blob") instanceof Blob, "get() on existing name should return first value");
  is(f.get("blob").type, 'text/plain', "get() on existing name should return first value");
  ok(f.get("file") instanceof File, "get() on existing name should return first value");
  is(f.get("file").name, 'testname', "get() on existing name should return first value");

  is(f.get("nonexistent"), null, "get() on non-existent name should return null.");
}

function testGetAll() {
  var f = new FormData();
  f.append("other", "value");
  f.append("foo", "bar");
  f.append("foo", "bar2");
  f.append("foo", new Blob(["hey"], { type: 'text/plain' }));

  var arr = f.getAll("foo");
  is(arr.length, 3, "getAll() should retrieve all matching entries.");
  is(arr[0], "bar", "values should match and be in order");
  is(arr[1], "bar2", "values should match and be in order");
  ok(arr[2] instanceof Blob, "values should match and be in order");

  is(f.get("nonexistent"), null, "get() on non-existent name should return null.");
}

function testDelete() {
  var f = new FormData();
  f.append("other", "value");
  f.append("foo", "bar");
  f.append("foo", "bar2");
  f.append("foo", new Blob(["hey"], { type: 'text/plain' }));

  ok(f.has("foo"), "has() on existing name should be true.");
  f.delete("foo");
  ok(!f.has("foo"), "has() on deleted name should be false.");
  is(f.getAll("foo").length, 0, "all entries should be deleted.");

  is(f.getAll("other").length, 1, "other names should still be there.");
  f.delete("other");
  is(f.getAll("other").length, 0, "all entries should be deleted.");
}

function testSet() {
  var f = new FormData();

  f.set("other", "value");
  ok(f.has("other"), "set() on new name should be similar to append()");
  is(f.getAll("other").length, 1, "set() on new name should be similar to append()");

  f.append("other", "value2");
  is(f.getAll("other").length, 2, "append() should not replace existing entries.");

  f.append("foo", "bar");
  f.append("other", "value3");
  f.append("other", "value3");
  f.append("other", "value3");
  is(f.getAll("other").length, 5, "append() should not replace existing entries.");

  f.set("other", "value4");
  is(f.getAll("other").length, 1, "set() should replace existing entries.");
  is(f.getAll("other")[0], "value4", "set() should replace existing entries.");
}

function testIterate() {
  todo(false, "Implement this in Bug 1085284.");
}

function testFilename() {
  var f = new FormData();
  // Spec says if a Blob (which is not a File) is added, the name parameter is set to "blob".
  f.append("blob", new Blob(["hi"]));
  is(f.get("blob").name, "blob", "Blob's filename should be blob.");

  // If a filename is passed, that should replace the original.
  f.append("blob2", new Blob(["hi"]), "blob2.txt");
  is(f.get("blob2").name, "blob2.txt", "Explicit filename should override \"blob\".");

  var file = new File(["hi"], "file1.txt");
  f.append("file1", file);
  // If a file is passed, the "create entry" algorithm should not create a new File, but reuse the existing one.
  is(f.get("file1"), file, "Retrieved File object should be original File object and not a copy.");
  is(f.get("file1").name, "file1.txt", "File's filename should be original's name if no filename is explicitly passed.");

  file = new File(["hi"], "file2.txt");
  f.append("file2", file, "fakename.txt");
  ok(f.get("file2") !== file, "Retrieved File object should be new File object if explicit filename is passed.");
  is(f.get("file2").name, "fakename.txt", "File's filename should be explicitly passed name.");
  f.append("file3", new File(["hi"], ""));
  is(f.get("file3").name, "", "File's filename is returned even if empty.");
}

function testSend(doneCb) {
  var xhr = new XMLHttpRequest();
  xhr.open("POST", "form_submit_server.sjs");
  xhr.onload = function () {
    var response = xhr.response;

    for (var entry of response) {
      is(entry.body, 'hey');
      is(entry.headers['Content-Type'], 'text/plain');
    }

    is(response[0].headers['Content-Disposition'],
        'form-data; name="empty"; filename="blob"');

    is(response[1].headers['Content-Disposition'],
        'form-data; name="explicit"; filename="explicit-file-name"');

    is(response[2].headers['Content-Disposition'],
        'form-data; name="explicit-empty"; filename=""');

    is(response[3].headers['Content-Disposition'],
        'form-data; name="file-name"; filename="testname"');

    is(response[4].headers['Content-Disposition'],
        'form-data; name="empty-file-name"; filename=""');

    is(response[5].headers['Content-Disposition'],
        'form-data; name="file-name-overwrite"; filename="overwrite"');

    doneCb();
  }

  var file, blob = new Blob(['hey'], {type: 'text/plain'});

  var fd = new FormData();
  fd.append("empty", blob);
  fd.append("explicit", blob, "explicit-file-name");
  fd.append("explicit-empty", blob, "");
  file = new File([blob], 'testname',  {type: 'text/plain'});
  fd.append("file-name", file);
  file = new File([blob], '',  {type: 'text/plain'});
  fd.append("empty-file-name", file);
  file = new File([blob], 'testname',  {type: 'text/plain'});
  fd.append("file-name-overwrite", file, "overwrite");
  xhr.responseType = 'json';
  xhr.send(fd);
}

function runTest(doneCb) {
  testHas();
  testGet();
  testGetAll();
  testDelete();
  testSet();
  testIterate();
  testFilename();
  // Finally, send an XHR and verify the response matches.
  testSend(doneCb);
}

