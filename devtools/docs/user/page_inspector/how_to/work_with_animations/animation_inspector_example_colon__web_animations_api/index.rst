===============================================
Animation inspector example: Web Animations API
===============================================

Firefox Logo Animation
**********************

Example animation using the `Web Animations API <https://developer.mozilla.org/en-US/docs/Web/API/Web_Animations_API>`_.


HTML Content
------------

.. code-block:: html

  <div class="channel">
    <img src="developer.png" id="icon"/>
    <span id="note">Firefox Developer Edition</span>
  </div>


CSS Content
-----------

.. code-block:: css

  .channel {
    padding: 2em;
    margin: 0.5em;
    box-shadow: 1px 1px 5px #808080;
    margin: 1.5em;
  }

  .channel > * {
    vertical-align: middle;
    line-height: normal;
  }

  #icon {
    width: 50px;
    height: 50px;
    filter: grayscale(100%);
  }

  #note {
    margin-left: 1em;
    font: 1.5em "Open Sans",Arial,sans-serif;
    overflow: hidden;
    white-space: nowrap;
    display: inline-block;
    opacity: 0;
    width: 0;
  }


.. _page-inspector-work-with-animations-web-example-js-content:

JavaScript Content
------------------

.. code-block:: JavaScript

  var iconKeyframeSet = [
    { transform: 'scale(1)', filter: 'grayscale(100%)'},
    { filter:  'grayscale(100%)', offset: 0.333},
    { transform: 'scale(1.5)', offset: 0.666 },
    { transform: 'scale(1.5)', filter: 'grayscale(0%)'}
  ];

  var noteKeyframeSet = [
    { opacity: '0', width: '0'},
    { opacity: '1', width: '300px'}
  ];

  var iconKeyframeOptions = {
    duration: 750,
    fill: 'forwards',
    easing: 'ease-in',
    endDelay: 100
  }

  var noteKeyframeOptions = {
    duration: 500,
    fill: 'forwards',
    easing: 'ease-out',
    delay: 150
  }

  var icon = document.getElementById("icon");
  var note = document.getElementById("note");

  var iconAnimation = icon.animate(iconKeyframeSet, iconKeyframeOptions);
  var noteAnimation = note.animate(noteKeyframeSet, noteKeyframeOptions);

  iconAnimation.pause();
  noteAnimation.pause();

  var firstTime = true;

  function animateChannel(e) {
   if (e.button != 0) {
   return;
   }
    if (e.target.id != "icon") {
      return;
    }
    if (firstTime) {
      iconAnimation.play();
      noteAnimation.play();
      firstTime = false;
    } else {
      iconAnimation.reverse();
      noteAnimation.reverse();
    }
  }

  document.addEventListener("click", animateChannel);
