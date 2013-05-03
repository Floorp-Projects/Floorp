  SimpleTest.waitForExplicitFinish();

  function frameLoaded() {
    var testframe = document.getElementById('testframe');
    var embed = document.getElementsByTagName('embed')[0];
    if (undefined === embed)
      embed = document.getElementsByTagName('object')[0];

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
    SimpleTest.finish();
  }
