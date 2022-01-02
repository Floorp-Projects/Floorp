===========
Performance
===========

The Performance tool gives you insight into your site's general responsiveness, JavaScript and layout performance. With the Performance tool you create a recording, or profile, of your site over a period of time. The tool then shows you an :ref:`overview <performance-ui-tour-waterfall-overview>` of the things the browser was doing to render your site over the profile, and a graph of the :doc:`frame rate <frame_rate/index>` over the profile.

You get four sub-tools to examine aspects of the profile in more detail:


- the :doc:`Waterfall <waterfall/index>` shows the different operations the browser was performing, such as executing layout, JavaScript, repaints, and garbage collection
- the :doc:`Call Tree <call_tree/index>` shows the JavaScript functions in which the browser spent most of its time
- the :doc:`Flame Chart <flame_chart/index>` shows the JavaScript call stack over the course of the recording
- the :doc:`Allocations <allocations/index>` view shows the heap allocations made by your code over the course of the recording. This view only appears if you checked "Record Allocations" in the Performance tool settings.

.. raw:: html

  <iframe width="560" height="315" src="https://www.youtube.com/embed/WBmttwfA_k8" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>
  <br/>
  <br/>

Getting started
***************

:doc:`UI Tour <ui_tour/index>`
  To find your way around the Performance tool, here's a quick tour of the UI.

:doc:`How to <how_to/index>`
  Basic tasks: open the tool, create, save, load, and configure recordings

Components of the Performance tool
**********************************

:doc:`Frame rate <frame_rate/index>`
  Understand your site's overall responsiveness.
:doc:`Call Tree <call_tree/index>`
  Find bottlenecks in your site's JavaScript.
:doc:`Allocations <allocations/index>`
  See the allocations made by your code over the course of the recording.
:doc:`Waterfall <waterfall/index>`
  Understand the work the browser's doing as the user interacts with your site.
:doc:`Flame Chart <flame_chart/index>`
  See which JavaScript functions are executing, and when, over the course of the recording.

Scenarios
*********

:doc:`Animating CSS properties <scenarios/animating_css_properties/index>`
  Uses the Waterfall to understand how the browser updates a page, and how animating different CSS properties can affect performance.
:doc:`Intensive JavaScript <scenarios/intensive_javascript/index>`
  Uses the frame rate and Waterfall tools to highlight performance problems caused by long-running JavaScript, and how using workers can help in this situation.
