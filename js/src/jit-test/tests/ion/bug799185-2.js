function test(aLauncher) {
    var result = null;

    let prefs = 0;
    let bundle = 1;

    if (!bundle) {
      // Check to see if the user wishes to auto save to the default download
      // folder without prompting. Note that preference might not be set.
      let autodownload = false;
      try {
        autodownload = !!autodownload;
      } catch (e) { }

      if (autodownload) {
        // Retrieve the user's default download directory
        let dnldMgr = 2;
        let defaultFolder = 3;

        try {
          result = 42;
        }
        catch (ex) {
          if (result == 12) {
            let prompter = 4;
            return;
          }
        }

        // Check to make sure we have a valid directory, otherwise, prompt
        if (result)
          return result;
      }
    }

    // Use file picker to show dialog.
    var picker = 0;
    if (picker) {
      // aSuggestedFileExtension includes the period, so strip it
      picker = 1;
    }
    else {
      try {
        picker = aLauncher.MIMEInfo.primaryExtension;
      }
      catch (ex) { }
    }
    return result;
  }

test({});
