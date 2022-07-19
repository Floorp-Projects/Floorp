================
Use a source map
================

The JavaScript sources executed by the browser are often transformed in some way from the original sources created by a developer. For example:

- sources are often combined and `minified <https://en.wikipedia.org/wiki/Minification_(programming)>`_ to make delivering them from the server more efficient.

- JavaScript running in a page is often machine-generated, as when compiled from a language like `CoffeeScript <https://coffeescript.org/>`_ or `TypeScript <https://www.typescriptlang.org/>`_

In these situations, it's much easier to debug the original source, rather than the source in the transformed state that the browser has downloaded. A `source map <https://www.html5rocks.com/en/tutorials/developertools/sourcemaps/>`_ is a file that maps from the transformed source to the original source, enabling the browser to reconstruct the original source and present the reconstructed original in the debugger.

To enable the debugger to work with a source map, you must:

- generate the source map
- include a comment in the transformed file, that points to the source map. The comment's syntax is like this:

.. code-block:: JavaScript

  //# sourceMappingURL=http://example.com/path/to/your/sourcemap.map

.. raw:: html

  <iframe width="560" height="315" src="https://www.youtube.com/embed/Fqd15gHC4Pg" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>
  <br/>
  <br/>

In the video above we load https://mdn.github.io/devtools-examples/sourcemaps-in-console/index.html. This page loads a source called "main.js" that was originally written in CoffeeScript and compiled to JavaScript. The compiled source contains a comment like this, that points to a source map:

.. code-block:: JavaScript

  //# sourceMappingURL=main.js.map

In the Debugger's :ref:`source list pane <debugger-ui-tour-source-list-pane>`, the original CoffeeScript source now appears as "main.coffee", and we can debug it just like any other source.
