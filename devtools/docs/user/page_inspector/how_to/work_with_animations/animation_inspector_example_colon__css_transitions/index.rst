============================================
Animation inspector example: CSS transitions
============================================

Firefox Logo Animation
**********************

Example animation using `CSS transitions <https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Transitions/Using_CSS_transitions>`_.

HTML Content
------------

.. code-block:: html

  <div class="channel">
    <img src="developer.png" class="icon"/>
    <span class="note">Firefox Developer Edition</span>
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

  .icon {
    width: 50px;
    height: 50px;
    filter: grayscale(100%);
    transition: transform 750ms ease-in, filter 750ms ease-in-out;
  }

  .note {
    margin-left: 1em;
    font: 1.5em "Open Sans",Arial,sans-serif;
    overflow: hidden;
    white-space: nowrap;
    display: inline-block;

    opacity: 0;
    width: 0;
    transition: opacity 500ms 150ms, width 500ms 150ms;
  }

  .icon#selected {
    filter: grayscale(0%);
    transform: scale(1.5);
  }

  .icon#selected+span {
    opacity: 1;
    width: 300px;
  }


JavaScript Content
------------------

.. code-block:: JavaScript

  function toggleSelection(e) {
    if (e.button != 0) {
      return;
    }
    if (e.target.classList.contains("icon")) {
      var wasSelected = (e.target.getAttribute("id") == "selected");
      clearSelection();
      if (!wasSelected) {
        e.target.setAttribute("id", "selected");
      }
    }
  }

  function clearSelection() {
    var selected = document.getElementById("selected");
    if (selected) {
      selected.removeAttribute("id");
    }
  }

  document.addEventListener("click", toggleSelection);
