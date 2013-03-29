/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var widgets = require('widget');
var pageMod = require('page-mod');
var data = require('self').data;
var panels = require('panel');
var simpleStorage = require('simple-storage');
var notifications = require("notifications");

/*
Global variables
* Boolean to indicate whether the add-on is switched on or not
* Array for all workers associated with the 'selector' page mod
* Array for all workers associated with the 'matcher' page mod
*/
var annotatorIsOn = false;
var selectors = [];
var matchers = [];

if (!simpleStorage.storage.annotations)
  simpleStorage.storage.annotations = [];

/*
Update the matchers: call this whenever the set of annotations changes
*/
function updateMatchers() {
  matchers.forEach(function (matcher) {
    matcher.postMessage(simpleStorage.storage.annotations);
  });
}

/*
Constructor for an Annotation object
*/
function Annotation(annotationText, anchor) {
  this.annotationText = annotationText;
  this.url = anchor[0];
  this.ancestorId = anchor[1];
  this.anchorText = anchor[2];
}

/*
Function to deal with a new annotation.
Create a new annotation object, store it, and
notify all the annotators of the change.
*/
function handleNewAnnotation(annotationText, anchor) {
  var newAnnotation = new Annotation(annotationText, anchor);
  simpleStorage.storage.annotations.push(newAnnotation);
  updateMatchers();
}

/*
Function to tell the selector page mod that the add-on has become (in)active
*/
function activateSelectors() {
  selectors.forEach(
    function (selector) {
      selector.postMessage(annotatorIsOn);
  });
}

/*
Toggle activation: update the on/off state and notify the selectors.
*/
function toggleActivation() {
  annotatorIsOn = !annotatorIsOn;
  activateSelectors();
  return annotatorIsOn;
}

function detachWorker(worker, workerArray) {
  var index = workerArray.indexOf(worker);
  if(index != -1) {
    workerArray.splice(index, 1);
  }
}

exports.main = function() {

/*
The widget provides a mechanism to switch the selector on or off, and to
view the list of annotations.

The selector is switched on/off with a left-click, and the list of annotations
is displayed on a right-click.
*/
  var widget = widgets.Widget({
    id: 'toggle-switch',
    label: 'Annotator',
    contentURL: data.url('widget/pencil-off.png'),
    contentScriptWhen: 'ready',
    contentScriptFile: data.url('widget/widget.js')
  });

  widget.port.on('left-click', function() {
    console.log('activate/deactivate');
    widget.contentURL = toggleActivation() ?
              data.url('widget/pencil-on.png') :
              data.url('widget/pencil-off.png');
  });

  widget.port.on('right-click', function() {
      console.log('show annotation list');
      annotationList.show();
  });

/*
The selector page-mod enables the user to select page elements to annotate.

It is attached to all pages but only operates if the add-on is active.

The content script highlights any page elements which can be annotated. If the
user clicks a highlighted element it sends a message to the add-on containing
information about the element clicked, which is called the anchor of the
annotation.

When we receive this message we assign the anchor to the annotationEditor and
display it.
*/
  var selector = pageMod.PageMod({
    include: ['*'],
    contentScriptWhen: 'ready',
    contentScriptFile: [data.url('jquery-1.4.2.min.js'),
                        data.url('selector.js')],
    onAttach: function(worker) {
      worker.postMessage(annotatorIsOn);
      selectors.push(worker);
      worker.port.on('show', function(data) {
        annotationEditor.annotationAnchor = data;
        annotationEditor.show();
      });
      worker.on('detach', function () {
        detachWorker(this, selectors);
      });
    }
  });

/*
The annotationEditor panel is the UI component used for creating
new annotations. It contains a text area for the user to
enter the annotation.

When we are ready to display the editor we assign its 'anchor' property
and call its show() method.

Its content script sends the content of the text area to the add-on
when the user presses the return key.

When we receives this message we create a new annotation using the anchor
and the text the user entered, store it, and hide the panel.
*/
  var annotationEditor = panels.Panel({
    width: 220,
    height: 220,
    contentURL: data.url('editor/annotation-editor.html'),
    contentScriptFile: data.url('editor/annotation-editor.js'),
    onMessage: function(annotationText) {
      if (annotationText)
        handleNewAnnotation(annotationText, this.annotationAnchor);
      annotationEditor.hide();
    },
    onShow: function() {
      this.postMessage('focus');
    }
  });

/*
The annotationList panel is the UI component that lists all the annotations
the user has entered.

On its 'show' event we pass it the array of annotations.

The content script creates the HTML elements for the annotations, and
intercepts clicks on the links, passing them back to the add-on to open them
in the browser.
*/
  var annotationList = panels.Panel({
    width: 420,
    height: 200,
    contentURL: data.url('list/annotation-list.html'),
    contentScriptFile: [data.url('jquery-1.4.2.min.js'),
                        data.url('list/annotation-list.js')],
    contentScriptWhen: 'ready',
    onShow: function() {
      this.postMessage(simpleStorage.storage.annotations);
    },
    onMessage: function(message) {
      require('tabs').open(message);
    }
  });

/*
We listen for the OverQuota event from simple-storage.
If it fires we just notify the user and delete the most
recent annotations until we are back in quota.
*/
  simpleStorage.on("OverQuota", function () {
    notifications.notify({
      title: 'Storage space exceeded',
      text: 'Removing recent annotations'});
    while (simpleStorage.quotaUsage > 1)
      simpleStorage.storage.annotations.pop();
  });

/*
The matcher page-mod locates anchors on web pages and prepares for the
annotation to be displayed.

It is attached to all pages, and when it is attached we pass it the complete
list of annotations. It looks for anchors in its page. If it finds one it
highlights the anchor and binds mouseenter/mouseout events to 'show' and 'hide'
messages to the add-on.

When the add-on receives the 'show' message it assigns the annotation text to
the annotation panel and shows it.

Note that the matcher is active whether or not the add-on is active:
'inactive' only means that the user can't create new add-ons, they can still
see old ones.
*/
  var matcher = pageMod.PageMod({
    include: ['*'],
    contentScriptWhen: 'ready',
    contentScriptFile: [data.url('jquery-1.4.2.min.js'),
                        data.url('matcher.js')],
    onAttach: function(worker) {
      if(simpleStorage.storage.annotations) {
        worker.postMessage(simpleStorage.storage.annotations);
      }
      worker.port.on('show', function(data) {
        annotation.content = data;
        annotation.show();
      });
      worker.port.on('hide', function() {
        annotation.content = null;
        annotation.hide();
      });
      worker.on('detach', function () {
        detachWorker(this, matchers);
      });
      matchers.push(worker);
    }
  });

/*
The annotation panel is the UI component that displays an annotation.

When we are ready to show it we assign its 'content' attribute to contain
the annotation text, and that gets sent to the content process in onShow().
*/
  var annotation = panels.Panel({
    width: 200,
    height: 180,
    contentURL: data.url('annotation/annotation.html'),
    contentScriptFile: [data.url('jquery-1.4.2.min.js'),
                        data.url('annotation/annotation.js')],
    contentScriptWhen: 'ready',
    onShow: function() {
      this.postMessage(this.content);
    }
  });

}
