// implements nsIObserver
var gEditorDocumentObserver =
{ 
  observe: function(aSubject, aTopic, aData)
  {
    // Should we allow this even if NOT the focused editor?
    var commandManager = EditorUtils.getCurrentCommandManager();
    if (commandManager != aSubject)
      return;

    var editor = EditorUtils.getCurrentEditor();
    gContentWindow = window.content;
    switch(aTopic)
    {
      case "obs_documentCreated":
        // placeholder; TBD
        dump("obs_documentCreated\n");

        // TBD : 1. DONE
        //       2. add override stylesheets if needed
        //       3. call "load" notifier callbacks
        //       4. update window title
        //       5. place initial selection
        //       6. check DTD strictness

        // Get state to see if document creation succeeded
        var params = EditorUtils.newCommandParams();
        if (!params)
          return;

        // TBD : RealTimeSpell.Init(editor, true);

        try {
          commandManager.getCommandState(aTopic, gContentWindow, params);
          var errorStringId = 0;
          var editorStatus = params.getLongValue("state_data");
          if (!editor && editorStatus == nsIEditingSession.eEditorOK)
          {
            dump("\n ****** NO EDITOR BUT NO EDITOR ERROR REPORTED ******* \n\n");
            editorStatus = nsIEditingSession.eEditorErrorUnknown;
          }

          switch (editorStatus)
          {
            case nsIEditingSession.eEditorErrorCantEditFramesets:
              errorStringId = "CantEditFramesetMsg";
              break;
            case nsIEditingSession.eEditorErrorCantEditMimeType:
              errorStringId = "CantEditMimeTypeMsg";
              break;
            case nsIEditingSession.eEditorErrorUnknown:
              errorStringId = "CantEditDocumentMsg";
              break;
            // Note that for "eEditorErrorFileNotFound, 
            // network code popped up an alert dialog, so we don't need to
          }
          if (errorStringId)
            PromptUtils.alertWithTitle("", L10NUtils.getString(errorStringId));
        } catch(e) { dump("EXCEPTION GETTING obs_documentCreated state "+e+"\n"); }

        // We have a bad editor -- nsIEditingSession will rebuild an editor
        //   with a blank page, so simply abort here
        if (editorStatus)
          return; 

        // Set window title
        UpdateWindowTitle();

        window.content.focus();
        break;

      case "cmd_setDocumentModified":
        dump("cmd_setDocumentModified " + EditorUtils.isDocumentModified() + "\n");
        gDialog.tabeditor.showCurrentTabAsModified(EditorUtils.isDocumentModified());    // || IsHTMLSourceChanged());

        // TBD : 1. update web navigation commands
        //       2. DONE

        break;

      case "obs_documentWillBeDestroyed":
        dump("obs_documentWillBeDestroyed\n");
        // placeholder; TBD
        break;

      case "obs_documentLocationChanged":
        dump("obs_documentLocationChanged\n");
        // TBD : 1. update base URL
        break;

      case "cmd_bold":
        // Update all style items
        // cmd_bold is a proxy; see EditorSharedStartup (above) for details
        window.updateCommands("style");
        // update the undo/redo items too
        window.updateCommands("undo");
        // and finally the main toolbar
        window.updateCommands("navigation");
        break;
    }
  }
}

function DeclareNotifiers()
{
  NotifierUtils.addNotifier("selection");
  NotifierUtils.addNotifier("style");
  NotifierUtils.addNotifier("beforeToSourceView");
  NotifierUtils.addNotifier("afterToSourceView");
  NotifierUtils.addNotifier("beforeFromSourceView");
  NotifierUtils.addNotifier("afterFromSourceView");
  NotifierUtils.addNotifier("beforeSaveOrPublish");
  NotifierUtils.addNotifier("load");
  NotifierUtils.addNotifier("attributeChange");
  NotifierUtils.addNotifier("inlineStyles");
  NotifierUtils.addNotifier("embeddedStyleSheet");
  NotifierUtils.addNotifier("linkedStyleSheet");
}

function RemoveNotifiers()
{
  NotifierUtils.cleanNotifier("selection");
  NotifierUtils.cleanNotifier("style");
  NotifierUtils.cleanNotifier("beforeToSourceView");
  NotifierUtils.cleanNotifier("afterToSourceView");
  NotifierUtils.cleanNotifier("beforeFromSourceView");
  NotifierUtils.cleanNotifier("afterFromSourceView");
  NotifierUtils.cleanNotifier("beforeSaveOrPublish");
  NotifierUtils.cleanNotifier("load");
  NotifierUtils.cleanNotifier("attributeChange");
  NotifierUtils.cleanNotifier("inlineStyles");
  NotifierUtils.cleanNotifier("embeddedStyleSheet");
  NotifierUtils.cleanNotifier("linkedStyleSheet");
}
