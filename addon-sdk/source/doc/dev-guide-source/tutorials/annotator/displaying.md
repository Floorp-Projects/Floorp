<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Displaying Annotations #

In this chapter we'll use a page-mod to locate elements of web pages that have
annotations associated with them, and a panel to display the annotations.

## Matcher page-mod ##

### Matcher Content Script ###

The content script for the matcher page-mod is initialized with a list
of all the annotations that the user has created.

When a page is loaded the matcher searches the DOM for elements that match
annotations. If it finds any it binds functions to that element's
[mouseenter](http://api.jquery.com/mouseenter/) and
[mouseleave](http://api.jquery.com/mouseleave/) events to send messages to the
`main` module, asking it to show or hide the annotation.

Like the selector, the matcher also listens for the window's `unload` event
and on unload sends a `detach` message to the `main` module, so the add-on
can clean it up.

The complete content script is here:

    self.on('message', function onMessage(annotations) {
      annotations.forEach(
        function(annotation) {
          if(annotation.url == document.location.toString()) {
            createAnchor(annotation);
          }
      });

      $('.annotated').css('border', 'solid 3px yellow');

      $('.annotated').bind('mouseenter', function(event) {
        self.port.emit('show', $(this).attr('annotation'));
        event.stopPropagation();
        event.preventDefault();
      });

      $('.annotated').bind('mouseleave', function() {
        self.port.emit('hide');
      });
    });

    function createAnchor(annotation) {
      annotationAnchorAncestor = $('#' + annotation.ancestorId);
      annotationAnchor = $(annotationAnchorAncestor).parent().find(
                         ':contains(' + annotation.anchorText + ')').last();
      $(annotationAnchor).addClass('annotated');
      $(annotationAnchor).attr('annotation', annotation.annotationText);
    }

Save this in `data` as `matcher.js`.

### Updating main.js ###

First, initialize an array to hold workers associated with the matcher's
content scripts:

    var matchers = [];

In the `main` function, add the code to create the matcher:

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

When a new page is loaded the function assigned to `onAttach` is called. This
function:

* initializes the content script instance with the current set of
annotations
* provides a handler for messages from that content script, handling the three
messages - `show`, `hide` and `detach` - that the content script might send
* adds the worker to an array, so we it can send messages back later.

Then in the module's scope implement a function to update the matcher's
workers, and edit `handleNewAnnotation` to call this new function when the
user enters a new annotation:

    function updateMatchers() {
      matchers.forEach(function (matcher) {
        matcher.postMessage(simpleStorage.storage.annotations);
      });
    }

<br>

    function handleNewAnnotation(annotationText, anchor) {
      var newAnnotation = new Annotation(annotationText, anchor);
      simpleStorage.storage.annotations.push(newAnnotation);
      updateMatchers();
    }
<br>

## Annotation panel ##

The annotation panel just shows the content of an annotation.

There are two files associated with the annotation panel:

* a simple HTML file to use as a template
* a simple content script to build the panel's content

These files will live in a new subdirectory of `data` which we'll call
`annotation`.

### Annotation panel HTML ###

<script type="syntaxhighlighter" class="brush: html"><![CDATA[
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
	"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
	<title>Annotation</title>
	<style type="text/css" media="all">

	body {
		font: 100% arial, helvetica, sans-serif;
		background-color: #F5F5F5;
	}

	div {
		text-align:left;
	}

	</style>

</head>

<body>

<div id = "annotation">
</div>

</body>
</html>
]]>
</script>

Save this in `data/annotation` as `annotation.html`.

### Annotation panel Content Script ###

The annotation panel has a minimal content script that sets the text:

     self.on('message', function(message) {
      $('#annotation').text(message);
    });

Save this in `data/annotation` as `annotation.js`.

### Updating main.js ###

Finally, update `main.js` with the code to construct the annotation panel:

    var annotation = panels.Panel({
      width: 200,
      height: 180,
      contentURL: data.url('annotation/annotation.html'),
      contentScriptFile: [data.url('jquery-1.4.2.min.js'),
                          data.url('annotation/annotation.js')],
      onShow: function() {
        this.postMessage(this.content);
      }
    });

Execute `cfx run` one last time. Activate the annotator and enter an
annotation. You should see a yellow border around the item you annotated:

<img class="image-center"
src="static-files/media/annotator/matcher.png" alt="Annotator Matcher">
<br>

When you move your mouse over the item, the annotation should appear:

<img class="image-center"
src="static-files/media/annotator/annotation-panel.png" alt="Annotation Panel">
<br>

Obviously this add-on isn't complete yet. It could do with more beautiful
styling, it certainly needs a way to delete annotations, it should deal with
`OverQuota` more reliably, and the matcher could be made to match more
reliably.

But we hope this gives you an idea of the things that are possible with the
modules in the SDK.
