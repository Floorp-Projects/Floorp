==================
Debug eval sources
==================

You can debug JavaScript code that is evaluated dynamically, either as a string passed to `eval() <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/eval>`_ or as a string passed to the `Function <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function>`_ constructor.

In the video below, we load a page containing a source like this:

.. code-block:: javascript

  var script = `function foo() {
    console.log('called foo');
  }
  //# sourceURL=my-foo.js`;

  eval(script);

  var button = document.getElementById("foo");
  button.addEventListener("click", foo, false);


The evaluated string is given the name "my-foo.js" using the ``//# sourceURL`` directive. This source is then listed in the :ref:`source list pane <debugger-ui-tour-source-list-pane>`, and can be opened and debugged like any other source.

.. raw:: html

  <iframe width="560" height="315" src="https://www.youtube.com/embed/AkvN40-y1NE" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>
  <br/>
  <br/>

The name of the source will also appear in stack traces appearing in the :doc:`Web Console <../../../web_console/index>`.

The debugger will also stop at `debugger; <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/debugger>`_ statements in unnamed eval sources.
