<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Storing Annotations #

Now we are able to create annotations, let's store them using the
[`simple-storage`](modules/sdk/simple-storage.html) module. In
this chapter we will cover three topics relating to persistent storage:

* using `simple-storage` to persist objects
* handling exhaustion of the storage quota allocated to you
* respecting Private Browsing

## Storing New Annotations ##

In this section we are only touching the `main.js` file.

First, import the `simple-storage` module with a declaration like:

    var simpleStorage = require('sdk/simple-storage');

In the module scope, initialize an array which will contain the stored annotations:

    if (!simpleStorage.storage.annotations)
      simpleStorage.storage.annotations = [];

Now we'll add a function to the module scope which deals with a new
annotation. The annotation is composed of the text the user entered and the
"annotation anchor", which consists of the URL, element ID and element content:

    function handleNewAnnotation(annotationText, anchor) {
      var newAnnotation = new Annotation(annotationText, anchor);
      simpleStorage.storage.annotations.push(newAnnotation);
    }

This function calls a constructor for an `Annotation` object, which we also
need to supply:

    function Annotation(annotationText, anchor) {
      this.annotationText = annotationText;
      this.url = anchor[0];
      this.ancestorId = anchor[1];
      this.anchorText = anchor[2];
    }

Now we need to link this code to the annotation editor, so that when the user
presses the return key in the editor, we create and store the new annotation:

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

## Listing Stored Annotations ##

To prove that this works, let's implement the part of the add-on that displays
all the previously entered annotations. This is implemented as a panel that's
shown in response to the widget's `right-click` message.

The panel has three new files associated with it:

* a content-script which builds the panel content
* a simple HTML file used as a template for the panel's content
* a simple CSS file to provide some basic styling.

These three files can all go in a new subdirectory of `data` which we will call `list`.

### Annotation List Content Script ###

Here's the annotation list's content script:

    self.on("message", function onMessage(storedAnnotations) {
      var annotationList = $('#annotation-list');
      annotationList.empty();
      storedAnnotations.forEach(
        function(storedAnnotation) {
          var annotationHtml = $('#template .annotation-details').clone();
          annotationHtml.find('.url').text(storedAnnotation.url)
                                     .attr('href', storedAnnotation.url);
          annotationHtml.find('.url').bind('click', function(event) {
            event.stopPropagation();
            event.preventDefault();
            self.postMessage(storedAnnotation.url);
          });
          annotationHtml.find('.selection-text')
                        .text(storedAnnotation.anchorText);
          annotationHtml.find('.annotation-text')
                        .text(storedAnnotation.annotationText);
          annotationList.append(annotationHtml);
        });
    });

It builds the DOM for the panel from the array of annotations it is given.

The user will be able to click links in the panel, but we want to open them in
the main browser window rather than the panel. So the content script binds a
click handler to the links which will send the URL to the add-on.

Save this file in `data/list` as `annotation-list.js`.

### Annotation List HTML and CSS ###

Here's the HTML for the annotation list:

<pre class="brush: html">
&lt;html&gt;
&lt;head&gt;
  &lt;meta http-equiv="Content-type" content="text/html; charset=utf-8" /&gt;
  &lt;title&gt;Saved annotations&lt;/title&gt;
  &lt;link rel="stylesheet" type="text/css" href="annotation-list.css" /&gt;
&lt;/head&gt;

&lt;body&gt;

&lt;div id="annotation-list"&gt;
&lt;/div&gt;

&lt;div id="template"&gt;
  &lt;div class="annotation-details"&gt;
    &lt;a class="url"&gt;&lt;/a&gt;
    &lt;div class="selection-text"&gt;&lt;/div&gt;
    &lt;div class="annotation-text"&gt;&lt;/div&gt;
  &lt;/div&gt;
&lt;/div&gt;

&lt;/body&gt;

&lt;/html&gt;

</pre>

Here's the corresponding CSS:

<script type="syntaxhighlighter" class="brush: css"><![CDATA[
#annotation-list .annotation-details
  {
  padding: 10px;
  margin: 10px;
  border: solid 3px #EEE;
  background-color: white;
  }

#annotation-list .url, .selection-text, .annotation-text
  {
  padding: 5px;
  margin: 5px;
  }

#annotation-list .selection-text,#annotation-list .annotation-text
  {
  border: solid 1px #EEE;
  }

#annotation-list .annotation-text
  {
  font-style: italic;
  }

body
  {
  background-color: #F5F5F5;
  font: 100% arial, helvetica, sans-serif;
  }

h1
  {
  font-family: georgia,serif;
  font-size: 1.5em;
  text-align:center;
  }
]]>
</script>

Save these in `data/list` as `annotation-list.html` and `annotation-list.css`
respectively.

### Updating main.js ###

Here's the code to create the panel, which can go in the `main` function.

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
        require('sdk/tabs').open(message);
      }
    });

Since this panel's content script uses jQuery we will pass that in too: again,
make sure the name of it matches the version of jQuery you downloaded.

When the panel is shown we send it the array of stored annotations. When the
panel sends us a URL we use the `tabs` module to open it in a new tab.

Finally we need to connect this to the widget's `right-click` message:

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

This time execute `cfx xpi` to build the XPI for the add-on, and install it in
Firefox. Activate the add-on, add an annotation, and then right-click the
widget. You should see something like this:

<img class="image-center"
src="static-files/media/annotator/annotation-list.png" alt="Annotation List">
<br>

<span class="aside">
Until now we've always run `cfx run` rather than building an XPI and installing
the add-on in Firefox. If the annotation does not reappear when you restart
Firefox, double check you installed the add-on and didn't just use `cfx run`
again.</span>
Restart Firefox, right-click the widget again, and check that the annotation
is still there.

## Responding To OverQuota events ##

Add-ons have a limited quota of storage space. If the add-on exits while
it is over quota, any data stored since the last time it was in quota will not
be persisted.

So we want to listen to the `OverQuota` event emitted by `simple-storage` and
respond to it. Add the following to your add-on's `main` function:

    simpleStorage.on("OverQuota", function () {
      notifications.notify({
        title: 'Storage space exceeded',
        text: 'Removing recent annotations'});
      while (simpleStorage.quotaUsage > 1)
        simpleStorage.storage.annotations.pop();
    });

Because we use a notification to alert the user, we need to import the
`notifications` module:

    var notifications = require("sdk/notifications");

(It should be obvious that this is an incredibly unhelpful way to deal with the
problem. A real add-on should give the user a chance to choose which data to
keep, and prevent the user from adding any more data until the add-on is back
under quota.)

## Respecting Private Browsing ##

Since annotations record the user's browsing history we should avoid recording
annotations in private windows.

There's a very simple way to do this: do nothing. By omitting the
[`"private-browsing"` key](dev-guide/package-spec.html#permissions) from the
annotator's "package.json" file, the annotator opts out of private browsing
altogether.

This means that its widget will not appear on any private windows and its
selector and matcher content scripts won't run, so the user won't be able to
enter any annotations in private windows.

Try it: execute cfx run and open a new private window: you should no longer
see the annotator's widget.

Now we can create and store annotations, the last piece is to
[display them when the user loads the page](dev-guide/tutorials/annotator/displaying.html).