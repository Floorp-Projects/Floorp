<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Implementing the Widget #

We want the widget to do two things:

<span class="aside">
[Bug 634712](https://bugzilla.mozilla.org/show_bug.cgi?id=634712) asks that
the widget API should emit separate, or at least distinguishable, events for
left and right mouse clicks, and when it is fixed this widget won't need a
content script any more.</span>

* On a left-click, the widget should activate or deactivate the annotator.
* On a right-click, the widget should display a list of all the annotations
the user has created.

Because the widget's `click` event does not distinguish left and right mouse
clicks, we'll use a content script to capture the click events and send the
corresponding message back to our add-on.

The widget will have two icons: one to display when it's active, one to display
when it's inactive.

So there are three files we'll need to create: the widget's content script and
its two icons.

Inside the `data` subdirectory create another subdirectory `widget`. We'll
keep the widget's files here. (Note that this isn't mandatory: you could just
keep them all under `data`.  But it seems tidier this way.)

## The Widget's Content Script ##

The widget's content script just listens for left- and right- mouse clicks and
posts the corresponding message to the add-on code:

    this.addEventListener('click', function(event) {
      if(event.button == 0 && event.shiftKey == false)
        self.port.emit('left-click');

      if(event.button == 2 || (event.button == 0 && event.shiftKey == true))
        self.port.emit('right-click');
        event.preventDefault();
    }, true);

Save this in your `data/widget` directory as `widget.js`.

## The Widget's Icons ##

You can copy the widget's icons from here:

<img style="margin-left:40px; margin-right:20px;" src="static-files/media/annotator/pencil-on.png" alt="Active Annotator">
<img style="margin-left:20px; margin-right:20px;" src="static-files/media/annotator/pencil-off.png" alt="Inactive Annotator">

(Or make your own if you're feeling creative.) Save them in your `data/widget` directory.

## main.js ##

Now in the `lib` directory open `main.js` and add the following code:

    var widgets = require('sdk/widget');
    var data = require('sdk/self').data;

    var annotatorIsOn = false;

    function toggleActivation() {
      annotatorIsOn = !annotatorIsOn;
      return annotatorIsOn;
    }

    exports.main = function() {

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
      });
    }

The annotator is inactive by default. It creates the widget and responds to
messages from the widget's content script by toggling its activation state.
<span class="aside">Note that due to
[bug 626326](https://bugzilla.mozilla.org/show_bug.cgi?id=626326) the add-on
bar's context menu is displayed, despite the `event.preventDefault()` call
in the widget's content script.</span>
Since we don't have any code to display annotations yet, we just log the
right-click events to the console.

Now from the `annotator` directory type `cfx run`. You should see the widget
in the add-on bar:

<div align="center">
<img src="static-files/media/annotator/widget-icon.png" alt="Widget Icon">
</div>
<br>

Left- and right-clicks should produce the appropriate debug output, and a
left-click should also change the widget icon to signal that it is active.

Next we'll add the code to
[create annotations](dev-guide/tutorials/annotator/creating.html).
