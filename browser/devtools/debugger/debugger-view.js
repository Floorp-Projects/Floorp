/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Victor Porof <vporof@mozilla.com> (original author)
 *   Mihai Sucan <mihai.sucan@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
"use strict";

/**
 * Object mediating visual changes and event listeners between the debugger and
 * the html view.
 */
let DebuggerView = {

  /**
   * An instance of SourceEditor.
   */
  editor: null,

  /**
   * Initializes the SourceEditor instance.
   */
  initializeEditor: function DV_initializeEditor() {
    let placeholder = document.getElementById("editor");

    let config = {
      mode: SourceEditor.MODES.JAVASCRIPT,
      showLineNumbers: true,
      readOnly: true,
      showAnnotationRuler: true,
      showOverviewRuler: true,
    };

    this.editor = new SourceEditor();
    this.editor.init(placeholder, config, this._onEditorLoad.bind(this));
  },

  /**
   * Removes the SourceEditor instance and added breakpoints.
   */
  destroyEditor: function DV_destroyEditor() {
    DebuggerController.Breakpoints.destroy();
    this.editor = null;
  },

  /**
   * The load event handler for the source editor. This method does post-load
   * editor initialization.
   */
  _onEditorLoad: function DV__onEditorLoad() {
    DebuggerController.Breakpoints.initialize();
  }
};

/**
 * Functions handling the scripts UI.
 */
function ScriptsView() {
  this._onScriptsChange = this._onScriptsChange.bind(this);
  this._onScriptsSearch = this._onScriptsSearch.bind(this);
}

ScriptsView.prototype = {

  /**
   * Removes all elements from the scripts container, leaving it empty.
   */
  empty: function DVS_empty() {
    while (this._scripts.firstChild) {
      this._scripts.removeChild(this._scripts.firstChild);
    }
  },

  /**
   * Removes the input in the searchbox and unhides all the scripts.
   */
  clearSearch: function DVS_clearSearch() {
    this._searchbox.value = "";
    this._onScriptsSearch({});
  },

  /**
   * Checks whether the script with the specified URL is among the scripts
   * known to the debugger and shown in the list.
   *
   * @param string aUrl
   *        The script URL.
   * @return boolean
   */
  contains: function DVS_contains(aUrl) {
    if (this._tmpScripts.some(function(element) {
      return element.script.url == aUrl;
    })) {
      return true;
    }
    if (this._scripts.getElementsByAttribute("value", aUrl).length > 0) {
      return true;
    }
    return false;
  },

  /**
   * Checks whether the script with the specified label is among the scripts
   * known to the debugger and shown in the list.
   *
   * @param string aLabel
   *        The script label.
   * @return boolean
   */
  containsLabel: function DVS_containsLabel(aLabel) {
    if (this._tmpScripts.some(function(element) {
      return element.label == aLabel;
    })) {
      return true;
    }
    if (this._scripts.getElementsByAttribute("label", aLabel).length > 0) {
      return true;
    }
    return false;
  },

  /**
   * Selects the script with the specified URL from the list.
   *
   * @param string aUrl
   *        The script URL.
   */
  selectScript: function DVS_selectScript(aUrl) {
    for (let i = 0, l = this._scripts.itemCount; i < l; i++) {
      if (this._scripts.getItemAtIndex(i).value == aUrl) {
        this._scripts.selectedIndex = i;
        return;
      }
    }
  },

  /**
   * Checks whether the script with the specified URL is selected in the list.
   *
   * @param string aUrl
   *        The script URL.
   */
  isSelected: function DVS_isSelected(aUrl) {
    if (this._scripts.selectedItem &&
        this._scripts.selectedItem.value == aUrl) {
      return true;
    }
    return false;
  },

  /**
   * Retrieve the URL of the selected script.
   * @return string | null
   */
  get selected() {
    return this._scripts.selectedItem ?
           this._scripts.selectedItem.value : null;
  },

  /**
   * Returns the list of labels in the scripts container.
   * @return array
   */
  get scriptLabels() {
    let labels = [];
    for (let i = 0, l = this._scripts.itemCount; i < l; i++) {
      labels.push(this._scripts.getItemAtIndex(i).label);
    }
    return labels;
  },

  /**
   * Returns the list of URIs for scripts in the page.
   * @return array
   */
  get scriptLocations() {
    let locations = [];
    for (let i = 0, l = this._scripts.itemCount; i < l; i++) {
      locations.push(this._scripts.getItemAtIndex(i).value);
    }
    return locations;
  },

  /**
   * Gets the number of visible (hidden=false) scripts in the container.
   * @return number
   */
  get visibleItemsCount() {
    let count = 0;
    for (let i = 0, l = this._scripts.itemCount; i < l; i++) {
      count += this._scripts.getItemAtIndex(i).hidden ? 0 : 1;
    }
    return count;
  },

  /**
   * Prepares a script to be added to the scripts container. This allows
   * for a large number of scripts to be batched up before being
   * alphabetically sorted and added in the container.
   * @see ScriptsView.commitScripts
   *
   * If aForceFlag is true, the script will be immediately inserted at the
   * necessary position in the container so that all the scripts remain sorted.
   * This can be much slower than batching up multiple scripts.
   *
   * @param string aLabel
   *        The simplified script location to be shown.
   * @param string aScript
   *        The source script.
   * @param boolean aForceFlag
   *        True to force the script to be immediately added.
   */
  addScript: function DVS_addScript(aLabel, aScript, aForceFlag) {
    // Batch the script to be added later.
    if (!aForceFlag) {
      this._tmpScripts.push({ label: aLabel, script: aScript });
      return;
    }

    // Find the target position in the menulist and insert the script there.
    for (let i = 0, l = this._scripts.itemCount; i < l; i++) {
      if (this._scripts.getItemAtIndex(i).label > aLabel) {
        this._createScriptElement(aLabel, aScript, i);
        return;
      }
    }
    // The script is alphabetically the last one.
    this._createScriptElement(aLabel, aScript, -1, true);
  },

  /**
   * Adds all the prepared scripts to the scripts container.
   * If a script already exists (was previously added), nothing happens.
   */
  commitScripts: function DVS_commitScripts() {
    let newScripts = this._tmpScripts;
    this._tmpScripts = [];

    if (!newScripts || !newScripts.length) {
      return;
    }
    newScripts.sort(function(a, b) {
      return a.label.toLowerCase() > b.label.toLowerCase();
    });

    for (let i = 0, l = newScripts.length; i < l; i++) {
      let item = newScripts[i];
      this._createScriptElement(item.label, item.script, -1, true);
    }
  },

  /**
   * Creates a custom script element and adds it to the scripts container.
   * If the script with the specified label already exists, nothing happens.
   *
   * @param string aLabel
   *        The simplified script location to be shown.
   * @param string aScript
   *        The source script.
   * @param number aIndex
   *        The index where to insert to new script in the container.
   *        Pass -1 to append the script at the end.
   * @param boolean aSelectIfEmptyFlag
   *        True to set the newly created script as the currently selected item
   *        if there are no other existing scripts in the container.
   */
  _createScriptElement: function DVS__createScriptElement(
    aLabel, aScript, aIndex, aSelectIfEmptyFlag)
  {
    // Make sure we don't duplicate anything.
    if (aLabel == "null" || this.containsLabel(aLabel)) {
      return;
    }

    let scriptItem =
      aIndex == -1 ? this._scripts.appendItem(aLabel, aScript.url)
                   : this._scripts.insertItemAt(aIndex, aLabel, aScript.url);

    scriptItem.setAttribute("tooltiptext", aScript.url);
    scriptItem.setUserData("sourceScript", aScript, null);

    if (this._scripts.itemCount == 1 && aSelectIfEmptyFlag) {
      this._scripts.selectedItem = scriptItem;
    }
  },

  /**
   * The click listener for the scripts container.
   */
  _onScriptsChange: function DVS__onScriptsChange() {
    let script = this._scripts.selectedItem.getUserData("sourceScript");
    this._preferredScript = script;
    DebuggerController.SourceScripts.showScript(script);
  },

  /**
   * The search listener for the scripts search box.
   */
  _onScriptsSearch: function DVS__onScriptsSearch(e) {
    let editor = DebuggerView.editor;
    let scripts = this._scripts;
    let rawValue = this._searchbox.value.toLowerCase();

    let rawLength = rawValue.length;
    let lastColon = rawValue.lastIndexOf(":");
    let lastAt = rawValue.lastIndexOf("@");

    let fileEnd = lastColon != -1 ? lastColon : lastAt != -1 ? lastAt : rawLength;
    let lineEnd = lastAt != -1 ? lastAt : rawLength;

    let file = rawValue.slice(0, fileEnd);
    let line = window.parseInt(rawValue.slice(fileEnd + 1, lineEnd)) || -1;
    let token = rawValue.slice(lineEnd + 1);

    // Presume we won't find anything.
    scripts.selectedItem = this._preferredScript;

    // If we're not searching for a file anymore, unhide all the scripts.
    if (!file) {
      for (let i = 0, l = scripts.itemCount; i < l; i++) {
        scripts.getItemAtIndex(i).hidden = false;
      }
    } else {
      for (let i = 0, l = scripts.itemCount, found = false; i < l; i++) {
        let item = scripts.getItemAtIndex(i);
        let target = item.value.toLowerCase();

        // Search is not case sensitive, and is tied to the url not the label.
        if (target.match(file)) {
          item.hidden = false;

          if (!found) {
            found = true;
            scripts.selectedItem = item;
          }
        }
        // Hide what doesn't match our search.
        else {
          item.hidden = true;
        }
      }
    }
    if (line > -1) {
      editor.setCaretPosition(line - 1);
    }
    if (token) {
      let offset = editor.find(token, { ignoreCase: true });
      if (offset > -1) {
        editor.setCaretPosition(0);
        editor.setCaretOffset(offset);
      }
    }
  },

  /**
   * The keyup listener for the scripts search box.
   */
  _onScriptsKeyUp: function DVS__onScriptsKeyUp(e) {
    if (e.keyCode === e.DOM_VK_ESCAPE) {
      DebuggerView.editor.focus();
      return;
    }

    if (e.keyCode === e.DOM_VK_RETURN || e.keyCode === e.DOM_VK_ENTER) {
      let editor = DebuggerView.editor;
      let offset = editor.findNext(true);
      if (offset > -1) {
        editor.setCaretPosition(0);
        editor.setCaretOffset(offset);
      }
    }
  },

  /**
   * The cached scripts container and search box.
   */
  _scripts: null,
  _searchbox: null,

  /**
   * Initialization function, called when the debugger is initialized.
   */
  initialize: function DVS_initialize() {
    this._scripts = document.getElementById("scripts");
    this._searchbox = document.getElementById("scripts-search");
    this._scripts.addEventListener("select", this._onScriptsChange, false);
    this._searchbox.addEventListener("select", this._onScriptsSearch, false);
    this._searchbox.addEventListener("input", this._onScriptsSearch, false);
    this._searchbox.addEventListener("keyup", this._onScriptsKeyUp, false);
    this.commitScripts();
  },

  /**
   * Destruction function, called when the debugger is shut down.
   */
  destroy: function DVS_destroy() {
    this._scripts.removeEventListener("select", this._onScriptsChange, false);
    this._searchbox.removeEventListener("select", this._onScriptsSearch, false);
    this._searchbox.removeEventListener("input", this._onScriptsSearch, false);
    this._searchbox.removeEventListener("keyup", this._onScriptsKeyUp, false);
    this._scripts = null;
    this._searchbox = null;
  }
};

/**
 * Functions handling the html stackframes UI.
 */
function StackFramesView() {
  this._onFramesScroll = this._onFramesScroll.bind(this);
  this._onCloseButtonClick = this._onCloseButtonClick.bind(this);
  this._onResumeButtonClick = this._onResumeButtonClick.bind(this);
  this._onStepOverClick = this._onStepOverClick.bind(this);
  this._onStepInClick = this._onStepInClick.bind(this);
  this._onStepOutClick = this._onStepOutClick.bind(this);
}

StackFramesView.prototype = {

  /**
   * Sets the current frames state based on the debugger active thread state.
   *
   * @param string aState
   *        Either "paused" or "attached".
   */
  updateState: function DVF_updateState(aState) {
    let resume = document.getElementById("resume");
    let status = document.getElementById("status");

    // If we're paused, show a pause label and a resume label on the button.
    if (aState == "paused") {
      status.textContent = L10N.getStr("pausedState");
      resume.label = L10N.getStr("resumeLabel");
    }
    // If we're attached, do the opposite.
    else if (aState == "attached") {
      status.textContent = L10N.getStr("runningState");
      resume.label = L10N.getStr("pauseLabel");
    }
    // No valid state parameter.
    else {
      status.textContent = "";
    }

    DebuggerView.Scripts.clearSearch();
  },

  /**
   * Removes all elements from the stackframes container, leaving it empty.
   */
  empty: function DVF_empty() {
    while (this._frames.firstChild) {
      this._frames.removeChild(this._frames.firstChild);
    }
  },

  /**
   * Removes all elements from the stackframes container, and adds a child node
   * with an empty text note attached.
   */
  emptyText: function DVF_emptyText() {
    // Make sure the container is empty first.
    this.empty();

    let item = document.createElement("div");

    // The empty node should look grayed out to avoid confusion.
    item.className = "empty list-item";
    item.appendChild(document.createTextNode(L10N.getStr("emptyStackText")));

    this._frames.appendChild(item);
  },

  /**
   * Adds a frame to the stackframes container.
   * If the frame already exists (was previously added), null is returned.
   * Otherwise, the newly created element is returned.
   *
   * @param number aDepth
   *        The frame depth specified by the debugger.
   * @param string aFrameNameText
   *        The name to be displayed in the list.
   * @param string aFrameDetailsText
   *        The details to be displayed in the list.
   * @return object
   *         The newly created html node representing the added frame.
   */
  addFrame: function DVF_addFrame(aDepth, aFrameNameText, aFrameDetailsText) {
    // Make sure we don't duplicate anything.
    if (document.getElementById("stackframe-" + aDepth)) {
      return null;
    }

    let frame = document.createElement("div");
    let frameName = document.createElement("span");
    let frameDetails = document.createElement("span");

    // Create a list item to be added to the stackframes container.
    frame.id = "stackframe-" + aDepth;
    frame.className = "dbg-stackframe list-item";

    // This list should display the name and details for the frame.
    frameName.className = "dbg-stackframe-name";
    frameDetails.className = "dbg-stackframe-details";
    frameName.appendChild(document.createTextNode(aFrameNameText));
    frameDetails.appendChild(document.createTextNode(aFrameDetailsText));

    frame.appendChild(frameName);
    frame.appendChild(frameDetails);

    this._frames.appendChild(frame);

    // Return the element for later use if necessary.
    return frame;
  },

  /**
   * Highlights a frame from the stackframe container as selected/deselected.
   *
   * @param number aDepth
   *        The frame depth specified by the debugger.
   * @param boolean aFlag
   *        True if the frame should be deselected, false otherwise.
   */
  highlightFrame: function DVF_highlightFrame(aDepth, aFlag) {
    let frame = document.getElementById("stackframe-" + aDepth);

    // The list item wasn't found in the stackframe container.
    if (!frame) {
      return;
    }

    // Add the 'selected' css class if the frame isn't already selected.
    if (!aFlag && !frame.classList.contains("selected")) {
      frame.classList.add("selected");
    }
    // Remove the 'selected' css class if the frame is already selected.
    else if (aFlag && frame.classList.contains("selected")) {
      frame.classList.remove("selected");
    }
  },

  /**
   * Deselects a frame from the stackframe container.
   *
   * @param number aDepth
   *        The frame depth specified by the debugger.
   */
  unhighlightFrame: function DVF_unhighlightFrame(aDepth) {
    this.highlightFrame(aDepth, true);
  },

  /**
   * Gets the current dirty state.
   *
   * @return boolean value
   *         True if should load more frames.
   */
  get dirty() {
    return this._dirty;
  },

  /**
   * Sets if the active thread has more frames that need to be loaded.
   *
   * @param boolean aValue
   *        True if should load more frames.
   */
  set dirty(aValue) {
    this._dirty = aValue;
  },

  /**
   * Listener handling the stackframes container click event.
   */
  _onFramesClick: function DVF__onFramesClick(aEvent) {
    let target = aEvent.target;

    while (target) {
      if (target.debuggerFrame) {
        DebuggerController.StackFrames.selectFrame(target.debuggerFrame.depth);
        return;
      }
      target = target.parentNode;
    }
  },

  /**
   * Listener handling the stackframes container scroll event.
   */
  _onFramesScroll: function DVF__onFramesScroll(aEvent) {
    // Update the stackframes container only if we have to.
    if (this._dirty) {
      let clientHeight = this._frames.clientHeight;
      let scrollTop = this._frames.scrollTop;
      let scrollHeight = this._frames.scrollHeight;

      // If the stackframes container was scrolled past 95% of the height,
      // load more content.
      if (scrollTop >= (scrollHeight - clientHeight) * 0.95) {
        this._dirty = false;

        DebuggerController.StackFrames.addMoreFrames();
      }
    }
  },

  /**
   * Listener handling the close button click event.
   */
  _onCloseButtonClick: function DVF__onCloseButtonClick() {
    DebuggerController.dispatchEvent("Debugger:Close");
  },

  /**
   * Listener handling the pause/resume button click event.
   */
  _onResumeButtonClick: function DVF__onResumeButtonClick() {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.resume();
    } else {
      DebuggerController.activeThread.interrupt();
    }
  },

  /**
   * Listener handling the step over button click event.
   */
  _onStepOverClick: function DVF__onStepOverClick() {
    DebuggerController.activeThread.stepOver();
  },

  /**
   * Listener handling the step in button click event.
   */
  _onStepInClick: function DVF__onStepInClick() {
    DebuggerController.activeThread.stepIn();
  },

  /**
   * Listener handling the step out button click event.
   */
  _onStepOutClick: function DVF__onStepOutClick() {
    DebuggerController.activeThread.stepOut();
  },

  /**
   * Specifies if the active thread has more frames which need to be loaded.
   */
  _dirty: false,

  /**
   * The cached stackframes container.
   */
  _frames: null,

  /**
   * Initialization function, called when the debugger is initialized.
   */
  initialize: function DVF_initialize() {
    let close = document.getElementById("close");
    let resume = document.getElementById("resume");
    let stepOver = document.getElementById("step-over");
    let stepIn = document.getElementById("step-in");
    let stepOut = document.getElementById("step-out");
    let frames = document.getElementById("stackframes");

    close.addEventListener("click", this._onCloseButtonClick, false);
    resume.addEventListener("click", this._onResumeButtonClick, false);
    stepOver.addEventListener("click", this._onStepOverClick, false);
    stepIn.addEventListener("click", this._onStepInClick, false);
    stepOut.addEventListener("click", this._onStepOutClick, false);
    frames.addEventListener("click", this._onFramesClick, false);
    frames.addEventListener("scroll", this._onFramesScroll, false);
    window.addEventListener("resize", this._onFramesScroll, false);

    this._frames = frames;
  },

  /**
   * Destruction function, called when the debugger is shut down.
   */
  destroy: function DVF_destroy() {
    let close = document.getElementById("close");
    let resume = document.getElementById("resume");
    let stepOver = document.getElementById("step-over");
    let stepIn = document.getElementById("step-in");
    let stepOut = document.getElementById("step-out");
    let frames = this._frames;

    close.removeEventListener("click", this._onCloseButtonClick, false);
    resume.removeEventListener("click", this._onResumeButtonClick, false);
    stepOver.removeEventListener("click", this._onStepOverClick, false);
    stepIn.removeEventListener("click", this._onStepInClick, false);
    stepOut.removeEventListener("click", this._onStepOutClick, false);
    frames.removeEventListener("click", this._onFramesClick, false);
    frames.removeEventListener("scroll", this._onFramesScroll, false);
    window.removeEventListener("resize", this._onFramesScroll, false);

    this._frames = null;
  }
};

/**
 * Functions handling the properties view.
 */
function PropertiesView() {
  this._addScope = this._addScope.bind(this);
  this._addVar = this._addVar.bind(this);
  this._addProperties = this._addProperties.bind(this);
}

PropertiesView.prototype = {

  /**
   * Adds a scope to contain any inspected variables.
   * If the optional id is not specified, the scope html node will have a
   * default id set as aName-scope.
   *
   * @param string aName
   *        The scope name (e.g. "Local", "Global" or "With block").
   * @param string aId
   *        Optional, an id for the scope html node.
   * @return object
   *         The newly created html node representing the added scope or null
   *         if a node was not created.
   */
  _addScope: function DVP__addScope(aName, aId) {
    // Make sure the parent container exists.
    if (!this._vars) {
      return null;
    }

    // Compute the id of the element if not specified.
    aId = aId || (aName.toLowerCase().trim().replace(" ", "-") + "-scope");

    // Contains generic nodes and functionality.
    let element = this._createPropertyElement(aName, aId, "scope", this._vars);

    // Make sure the element was created successfully.
    if (!element) {
      return null;
    }

    /**
     * @see DebuggerView.Properties._addVar
     */
    element.addVar = this._addVar.bind(this, element);

    // Return the element for later use if necessary.
    return element;
  },

  /**
   * Adds a variable to a specified scope.
   * If the optional id is not specified, the variable html node will have a
   * default id set as aScope.id->aName-variable.
   *
   * @param object aScope
   *        The parent scope element.
   * @param string aName
   *        The variable name.
   * @param string aId
   *        Optional, an id for the variable html node.
   * @return object
   *         The newly created html node representing the added var.
   */
  _addVar: function DVP__addVar(aScope, aName, aId) {
    // Make sure the scope container exists.
    if (!aScope) {
      return null;
    }

    // Compute the id of the element if not specified.
    aId = aId || (aScope.id + "->" + aName + "-variable");

    // Contains generic nodes and functionality.
    let element = this._createPropertyElement(aName, aId, "variable",
                                              aScope.querySelector(".details"));

    // Make sure the element was created successfully.
    if (!element) {
      return null;
    }

    /**
     * @see DebuggerView.Properties._setGrip
     */
    element.setGrip = this._setGrip.bind(this, element);

    /**
     * @see DebuggerView.Properties._addProperties
     */
    element.addProperties = this._addProperties.bind(this, element);

    // Setup the additional elements specific for a variable node.
    element.refresh(function() {
      let separator = document.createElement("span");
      let info = document.createElement("span");
      let title = element.querySelector(".title");
      let arrow = element.querySelector(".arrow");

      // Separator shouldn't be selectable.
      separator.className = "unselectable";
      separator.appendChild(document.createTextNode(": "));

      // The variable information (type, class and/or value).
      info.className = "info";

      title.appendChild(separator);
      title.appendChild(info);

    }.bind(this));

    // Return the element for later use if necessary.
    return element;
  },

  /**
   * Sets the specific grip for a variable.
   * The grip should contain the value or the type & class, as defined in the
   * remote debugger protocol. For convenience, undefined and null are
   * both considered types.
   *
   * @param object aVar
   *        The parent variable element.
   * @param object aGrip
   *        The primitive or object defining the grip, specifying
   *        the value and/or type & class of the variable (if the type
   *        is not specified, it will be inferred from the value).
   *        e.g. 42
   *             true
   *             "nasu"
   *             { type: "undefined" }
   *             { type: "null" }
   *             { type: "object", class: "Object" }
   * @return object
   *         The same variable.
   */
  _setGrip: function DVP__setGrip(aVar, aGrip) {
    // Make sure the variable container exists.
    if (!aVar) {
      return null;
    }
    if (aGrip === undefined) {
      aGrip = { type: "undefined" };
    }
    if (aGrip === null) {
      aGrip = { type: "null" };
    }

    let info = aVar.querySelector(".info") || aVar.target.info;

    // Make sure the info node exists.
    if (!info) {
      return null;
    }

    info.textContent = this._propertyString(aGrip);
    info.classList.add(this._propertyColor(aGrip));

    return aVar;
  },

  /**
   * Adds multiple properties to a specified variable.
   * This function handles two types of properties: data properties and
   * accessor properties, as defined in the remote debugger protocol spec.
   *
   * @param object aVar
   *        The parent variable element.
   * @param object aProperties
   *        An object containing the key: descriptor data properties,
   *        specifying the value and/or type & class of the variable,
   *        or 'get' & 'set' accessor properties.
   *        e.g. { "someProp0": { value: 42 },
   *               "someProp1": { value: true },
   *               "someProp2": { value: "nasu" },
   *               "someProp3": { value: { type: "undefined" } },
   *               "someProp4": { value: { type: "null" } },
   *               "someProp5": { value: { type: "object", class: "Object" } },
   *               "someProp6": { get: { type: "object", class: "Function" },
   *                              set: { type: "undefined" } }
   * @return object
   *         The same variable.
   */
  _addProperties: function DVP__addProperties(aVar, aProperties) {
    // For each property, add it using the passed object key/grip.
    for (let i in aProperties) {
      // Can't use aProperties.hasOwnProperty(i), because it may be overridden.
      if (Object.getOwnPropertyDescriptor(aProperties, i)) {

        // Get the specified descriptor for current property.
        let desc = aProperties[i];

        // As described in the remote debugger protocol, the value grip must be
        // contained in a 'value' property.
        let value = desc["value"];

        // For accessor property descriptors, the two grips need to be
        // contained in 'get' and 'set' properties.
        let getter = desc["get"];
        let setter = desc["set"];

        // Handle data property and accessor property descriptors.
        if (value !== undefined) {
          this._addProperty(aVar, [i, value]);
        }
        if (getter !== undefined || setter !== undefined) {
          let prop = this._addProperty(aVar, [i]).expand();
          prop.getter = this._addProperty(prop, ["get", getter]);
          prop.setter = this._addProperty(prop, ["set", setter]);
        }
      }
    }
    return aVar;
  },

  /**
   * Adds a property to a specified variable.
   * If the optional id is not specified, the property html node will have a
   * default id set as aVar.id->aKey-property.
   *
   * @param object aVar
   *        The parent variable element.
   * @param {Array} aProperty
   *        An array containing the key and grip properties, specifying
   *        the value and/or type & class of the variable (if the type
   *        is not specified, it will be inferred from the value).
   *        e.g. ["someProp0", 42]
   *             ["someProp1", true]
   *             ["someProp2", "nasu"]
   *             ["someProp3", { type: "undefined" }]
   *             ["someProp4", { type: "null" }]
   *             ["someProp5", { type: "object", class: "Object" }]
   * @param string aName
   *        Optional, the property name.
   * @paarm string aId
   *        Optional, an id for the property html node.
   * @return object
   *         The newly created html node representing the added prop.
   */
  _addProperty: function DVP__addProperty(aVar, aProperty, aName, aId) {
    // Make sure the variable container exists.
    if (!aVar) {
      return null;
    }

    // Compute the id of the element if not specified.
    aId = aId || (aVar.id + "->" + aProperty[0] + "-property");

    // Contains generic nodes and functionality.
    let element = this._createPropertyElement(aName, aId, "property",
                                              aVar.querySelector(".details"));

    // Make sure the element was created successfully.
    if (!element) {
      return null;
    }

    /**
     * @see DebuggerView.Properties._setGrip
     */
    element.setGrip = this._setGrip.bind(this, element);

    /**
     * @see DebuggerView.Properties._addProperties
     */
    element.addProperties = this._addProperties.bind(this, element);

    // Setup the additional elements specific for a variable node.
    element.refresh(function(pKey, pGrip) {
      let propertyString = this._propertyString(pGrip);
      let propertyColor = this._propertyColor(pGrip);
      let key = document.createElement("div");
      let value = document.createElement("div");
      let separator = document.createElement("span");
      let title = element.querySelector(".title");
      let arrow = element.querySelector(".arrow");

      // Use a key element to specify the property name.
      key.className = "key";
      key.appendChild(document.createTextNode(pKey));

      // Use a value element to specify the property value.
      value.className = "value";
      value.appendChild(document.createTextNode(propertyString));
      value.classList.add(propertyColor);

      // Separator shouldn't be selected.
      separator.className = "unselectable";
      separator.appendChild(document.createTextNode(": "));

      if ("undefined" !== typeof pKey) {
        title.appendChild(key);
      }
      if ("undefined" !== typeof pGrip) {
        title.appendChild(separator);
        title.appendChild(value);
      }

      // Make the property also behave as a variable, to allow
      // recursively adding properties to properties.
      element.target = {
        info: value
      };

      // Save the property to the variable for easier access.
      Object.defineProperty(aVar, pKey, { value: element,
                                          writable: false,
                                          enumerable: true,
                                          configurable: true });
    }.bind(this), aProperty);

    // Return the element for later use if necessary.
    return element;
  },

  /**
   * Returns a custom formatted property string for a type and a value.
   *
   * @param string | object aGrip
   *        The variable grip.
   * @return string
   *         The formatted property string.
   */
  _propertyString: function DVP__propertyString(aGrip) {
    if (aGrip && "object" === typeof aGrip) {
      switch (aGrip.type) {
        case "undefined":
          return "undefined";
        case "null":
          return "null";
        default:
          return "[" + aGrip.type + " " + aGrip.class + "]";
      }
    } else {
      switch (typeof aGrip) {
        case "string":
          return "\"" + aGrip + "\"";
        case "boolean":
          return aGrip ? "true" : "false";
        default:
          return aGrip + "";
      }
    }
    return aGrip + "";
  },

  /**
   * Returns a custom class style for a type and a value.
   *
   * @param string | object aGrip
   *        The variable grip.
   *
   * @return string
   *         The css class style.
   */
  _propertyColor: function DVP__propertyColor(aGrip) {
    if (aGrip && "object" === typeof aGrip) {
      switch (aGrip.type) {
        case "undefined":
          return "token-undefined";
        case "null":
          return "token-null";
      }
    } else {
      switch (typeof aGrip) {
        case "string":
          return "token-string";
        case "boolean":
          return "token-boolean";
        case "number":
          return "token-number";
      }
    }
    return "token-other";
  },

  /**
   * Creates an element which contains generic nodes and functionality used by
   * any scope, variable or property added to the tree.
   *
   * @param string aName
   *        A generic name used in a title strip.
   * @param string aId
   *        id used by the created element node.
   * @param string aClass
   *        Recommended style class used by the created element node.
   * @param object aParent
   *        The parent node which will contain the element.
   * @return object
   *         The newly created html node representing the generic elem.
   */
  _createPropertyElement: function DVP__createPropertyElement(aName, aId, aClass, aParent) {
    // Make sure we don't duplicate anything and the parent exists.
    if (document.getElementById(aId)) {
      return null;
    }
    if (!aParent) {
      return null;
    }

    let element = document.createElement("div");
    let arrow = document.createElement("span");
    let name = document.createElement("span");
    let title = document.createElement("div");
    let details = document.createElement("div");

    // Create a scope node to contain all the elements.
    element.id = aId;
    element.className = aClass;

    // The expand/collapse arrow.
    arrow.className = "arrow";
    arrow.style.visibility = "hidden";

    // The name element.
    name.className = "name unselectable";
    name.appendChild(document.createTextNode(aName || ""));

    // The title element, containing the arrow and the name.
    title.className = "title";
    title.addEventListener("click", function() { element.toggle(); }, true);

    // The node element which will contain any added scope variables.
    details.className = "details";

    title.appendChild(arrow);
    title.appendChild(name);

    element.appendChild(title);
    element.appendChild(details);

    aParent.appendChild(element);

    /**
     * Shows the element, setting the display style to "block".
     * @return object
     *         The same element.
     */
    element.show = function DVP_element_show() {
      element.style.display = "-moz-box";

      if ("function" === typeof element.onshow) {
        element.onshow(element);
      }
      return element;
    };

    /**
     * Hides the element, setting the display style to "none".
     * @return object
     *         The same element.
     */
    element.hide = function DVP_element_hide() {
      element.style.display = "none";

      if ("function" === typeof element.onhide) {
        element.onhide(element);
      }
      return element;
    };

    /**
     * Expands the element, showing all the added details.
     * @return object
     *         The same element.
     */
    element.expand = function DVP_element_expand() {
      arrow.setAttribute("open", "");
      details.setAttribute("open", "");

      if ("function" === typeof element.onexpand) {
        element.onexpand(element);
      }
      return element;
    };

    /**
     * Collapses the element, hiding all the added details.
     * @return object
     *         The same element.
     */
    element.collapse = function DVP_element_collapse() {
      arrow.removeAttribute("open");
      details.removeAttribute("open");

      if ("function" === typeof element.oncollapse) {
        element.oncollapse(element);
      }
      return element;
    };

    /**
     * Toggles between the element collapse/expand state.
     * @return object
     *         The same element.
     */
    element.toggle = function DVP_element_toggle() {
      element.expanded = !element.expanded;

      if ("function" === typeof element.ontoggle) {
        element.ontoggle(element);
      }
      return element;
    };

    /**
     * Shows the element expand/collapse arrow (only if necessary!).
     * @return object
     *         The same element.
     */
    element.showArrow = function DVP_element_showArrow() {
      if (details.childNodes.length) {
        arrow.style.visibility = "visible";
      }
      return element;
    };

    /**
     * Forces the element expand/collapse arrow to be visible, even if there
     * are no child elements.
     *
     * @param boolean aPreventHideFlag
     *        Prevents the arrow to be hidden when requested.
     * @return object
     *         The same element.
     */
    element.forceShowArrow = function DVP_element_forceShowArrow(aPreventHideFlag) {
      element._preventHide = aPreventHideFlag;
      arrow.style.visibility = "visible";
      return element;
    };

    /**
     * Hides the element expand/collapse arrow.
     * @return object
     *         The same element.
     */
    element.hideArrow = function DVP_element_hideArrow() {
      if (!element._preventHide) {
        arrow.style.visibility = "hidden";
      }
      return element;
    };

    /**
     * Returns if the element is visible.
     * @return boolean
     *         True if the element is visible.
     */
    Object.defineProperty(element, "visible", {
      get: function DVP_element_getVisible() {
        return element.style.display !== "none";
      },
      set: function DVP_element_setVisible(value) {
        if (value) {
          element.show();
        } else {
          element.hide();
        }
      }
    });

    /**
     * Returns if the element is expanded.
     * @return boolean
     *         True if the element is expanded.
     */
    Object.defineProperty(element, "expanded", {
      get: function DVP_element_getExpanded() {
        return arrow.hasAttribute("open");
      },
      set: function DVP_element_setExpanded(value) {
        if (value) {
          element.expand();
        } else {
          element.collapse();
        }
      }
    });

    /**
     * Removes all added children in the details container tree.
     * @return object
     *         The same element.
     */
    element.empty = function DVP_element_empty() {
      // this details node won't have any elements, so hide the arrow
      arrow.style.visibility = "hidden";
      while (details.firstChild) {
        details.removeChild(details.firstChild);
      }

      if ("function" === typeof element.onempty) {
        element.onempty(element);
      }
      return element;
    };

    /**
     * Removes the element from the parent node details container tree.
     * @return object
     *         The same element.
     */
    element.remove = function DVP_element_remove() {
      element.parentNode.removeChild(element);

      if ("function" === typeof element.onremove) {
        element.onremove(element);
      }
      return element;
    };

    /**
     * Generic function refreshing the internal state of the element when
     * it's modified (e.g. a child detail, variable, property is added).
     *
     * @param function aFunction
     *        The function logic used to modify the internal state.
     * @param array aArguments
     *        Optional arguments array to be applied to aFunction.
     */
    element.refresh = function DVP_element_refresh(aFunction, aArguments) {
      if ("function" === typeof aFunction) {
        aFunction.apply(this, aArguments);
      }

      let node = aParent.parentNode;
      let arrow = node.querySelector(".arrow");
      let children = node.querySelector(".details").childNodes.length;

      // If the parent details node has at least one element, set the
      // expand/collapse arrow visible.
      if (children) {
        arrow.style.visibility = "visible";
      } else {
        arrow.style.visibility = "hidden";
      }
    }.bind(this);

    // Return the element for later use and customization.
    return element;
  },

  /**
   * Returns the global scope container.
   */
  get globalScope() {
    return this._globalScope;
  },

  /**
   * Sets the display mode for the global scope container.
   *
   * @param boolean aFlag
   *        False to hide the container, true to show.
   */
  set globalScope(aFlag) {
    if (aFlag) {
      this._globalScope.show();
    } else {
      this._globalScope.hide();
    }
  },

  /**
   * Returns the local scope container.
   */
  get localScope() {
    return this._localScope;
  },

  /**
   * Sets the display mode for the local scope container.
   *
   * @param boolean aFlag
   *        False to hide the container, true to show.
   */
  set localScope(aFlag) {
    if (aFlag) {
      this._localScope.show();
    } else {
      this._localScope.hide();
    }
  },

  /**
   * Returns the with block scope container.
   */
  get withScope() {
    return this._withScope;
  },

  /**
   * Sets the display mode for the with block scope container.
   *
   * @param boolean aFlag
   *        False to hide the container, true to show.
   */
  set withScope(aFlag) {
    if (aFlag) {
      this._withScope.show();
    } else {
      this._withScope.hide();
    }
  },

  /**
   * Returns the closure scope container.
   */
  get closureScope() {
    return this._closureScope;
  },

  /**
   * Sets the display mode for the with block scope container.
   *
   * @param boolean aFlag
   *        False to hide the container, true to show.
   */
  set closureScope(aFlag) {
    if (aFlag) {
      this._closureScope.show();
    } else {
      this._closureScope.hide();
    }
  },

  /**
   * The cached variable properties container.
   */
  _vars: null,

  /**
   * Auto-created global, local, with block and closure scopes containing vars.
   */
  _globalScope: null,
  _localScope: null,
  _withScope: null,
  _closureScope: null,

  /**
   * Initialization function, called when the debugger is initialized.
   */
  initialize: function DVP_initialize() {
    this._vars = document.getElementById("variables");
    this._localScope = this._addScope(L10N.getStr("localScope")).expand();
    this._withScope = this._addScope(L10N.getStr("withScope")).hide();
    this._closureScope = this._addScope(L10N.getStr("closureScope")).hide();
    this._globalScope = this._addScope(L10N.getStr("globalScope"));
  },

  /**
   * Destruction function, called when the debugger is shut down.
   */
  destroy: function DVP_destroy() {
    this._vars = null;
    this._globalScope = null;
    this._localScope = null;
    this._withScope = null;
    this._closureScope = null;
  }
};

/**
 * Preliminary setup for the DebuggerView object.
 */
DebuggerView.Scripts = new ScriptsView();
DebuggerView.StackFrames = new StackFramesView();
DebuggerView.Properties = new PropertiesView();

/**
 * Export the source editor to the global scope for easier access in tests.
 */
Object.defineProperty(window, "editor", {
  get: function() { return DebuggerView.editor; }
});
