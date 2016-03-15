  SimpleTest.waitForExplicitFinish();

  function frameLoaded(finishWhenCalled = true, lastObject = false) {
    var testframe = document.getElementById('testframe');
    function getNode(list) {
      if (list.length === 0)
        return undefined;
      return lastObject ? list[list.length - 1] : list[0];
    }
    var embed = getNode(document.getElementsByTagName('embed'));
    if (undefined === embed)
      embed = getNode(document.getElementsByTagName('object'));

    // In the file:// URI case, this ends up being cross-origin.
    // Skip these checks in that case.
    if (testframe.contentDocument) {
      var content = testframe.contentDocument.body.innerHTML;
      if (!content.length)
        return;

      var filename = embed.getAttribute("src") ||
          embed.getAttribute("geturl") ||
          embed.getAttribute("geturlnotify") ||
          embed.getAttribute("data");

      var req = new XMLHttpRequest();
      req.open('GET', filename, false);
      req.overrideMimeType('text/plain; charset=x-user-defined');
      req.send(null);
      is(req.status, 200, "bad XMLHttpRequest status");
      is(content, req.responseText.replace(/\r\n/g, "\n"),
         "content doesn't match");
    }

    is(embed.getError(), "pass", "plugin reported error");
    if (finishWhenCalled) {
      SimpleTest.finish();
    }
  }
