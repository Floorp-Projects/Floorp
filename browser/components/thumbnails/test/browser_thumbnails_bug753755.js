/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that PageThumbsStorage.getFileForURL(url) doesn't implicitly
 * create the file's parent path.
 */
function runTests() {
  let url = "http://non.existant.url/";
  let file = PageThumbsStorage.getFileForURL(url);
  ok(!file.exists() && !file.parent.exists(), "file and path don't exist");

  let file = PageThumbsStorage.getFileForURL(url, {createPath: true});
  ok(!file.exists() && file.parent.exists(), "path exists, file doesn't");
}
