Extensions & Experiments
========================

This document describes address bar extensions and experiments: what they are,
how to run them, how to write them, and the processes involved in each.

The primary purpose right now for writing address bar extensions is to run
address bar experiments. But extensions are useful outside of experiments, and
not all experiments use extensions.

Like all Firefox extensions, address bar extensions use the WebExtensions_
framework.

.. _WebExtensions: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions

.. contents::
   :depth: 2


WebExtensions
-------------

**WebExtensions** is the name of Firefox's extension architecture. The "web"
part of the name hints at the fact that Firefox extensions are built using Web
technologies: JavaScript, HTML, CSS, and to a certain extent the DOM.

Individual extensions themselves often are referred to as *WebExtensions*. For
clarity and conciseness, this document will refer to WebExtensions as
*extensions*.

Why are we interested in extensions? Mainly because they're a powerful way to
run experiments in Firefox. See Experiments_ for more on that. In addition, we'd
also like to build up a robust set of APIs useful to extension authors, although
right now the API can only be used by Mozilla extensions.

WebExtensions are introduced and discussed in detail on `MDN
<WebExtensions_>`__. You'll need a lot of that knowledge in order to build
address bar extensions.

Developing Address Bar Extensions
---------------------------------

Overview
~~~~~~~~

The address bar WebExtensions API currently lives in two API namespaces,
``browser.urlbar`` and ``browser.experiments.urlbar``. The reason for this is
historical and is discussed in the `Developing Address Bar Extension APIs`_
section. As a consumer of the API, there are only two important things you need
to know:

* There's no meaningful difference between the APIs of the two namespaces.
  Their kinds of functions, events, and properties are similar.  You should
  think of the address bar API as one single API that happens to be split into
  two namespaces.

* However, there is a big difference between the two when it comes to setting up
  your extension to use them. This is discussed next.

The ``browser.urlbar`` API namespace is built into Firefox. It's a
**privileged API**, which means that only Mozilla-signed and temporarily
installed extensions can use it. The only thing your Mozilla extension needs to
do in order to use it is to request the ``urlbar`` permission in its
manifest.json, as illustrated `here <urlbarPermissionExample_>`__.

In contrast, the ``browser.experiments.urlbar`` API namespace is bundled inside
your extension. APIs that are bundled inside extensions are called
**experimental APIs**, and the extensions in which they're bundled are called
**WebExtension experiments**. As with privileged APIs, experimental APIs are
available only to Mozilla-signed and temporarily installed extensions.
("WebExtension experiments" is a term of art and shouldn't be confused with the
general notion of experiments that happen to use extensions.) For more on
experimental APIs and WebExtension experiments, see the `WebExtensions API
implementation documentation <webextAPIImplBasicsDoc_>`__.

Since ``browser.experiments.urlbar`` is bundled inside your extension, you'll
need to include it in your extension's repo by doing the following:

1. The implementation consists of two files, api.js_ and schema.json_. In your
   extension repo, create a *experiments/urlbar* subdirectory and copy the
   files there. See `this repo`__ for an example.

2. Add the following ``experiment_apis`` key to your manifest.json (see here__
   for an example in context)::

     "experiment_apis": {
       "experiments_urlbar": {
         "schema": "experiments/urlbar/schema.json",
         "parent": {
           "scopes": ["addon_parent"],
           "paths": [["experiments", "urlbar"]],
           "script": "experiments/urlbar/api.js"
         }
       }
     }

As mentioned, only Mozilla-signed and temporarily installed extensions can use
these two API namespaces. For information on running the extensions you develop
that use these namespaces, see `Running Address Bar Extensions`_.

.. _urlbarPermissionExample: https://github.com/0c0w3/urlbar-top-sites-experiment/blob/ac1517118bb7ee165fb9989834514b1082575c10/src/manifest.json#L24
.. _webextAPIImplBasicsDoc: https://firefox-source-docs.mozilla.org/toolkit/components/extensions/webextensions/basics.html
.. _api.js: https://searchfox.org/mozilla-central/source/browser/components/urlbar/tests/ext/api.js
.. _schema.json: https://searchfox.org/mozilla-central/source/browser/components/urlbar/tests/ext/schema.json
__ https://github.com/0c0w3/dynamic-result-type-extension/tree/master/src/experiments/urlbar
__ https://github.com/0c0w3/dynamic-result-type-extension/blob/0987da4b259b9fcb139b31d771883a2f822712b5/src/manifest.json#L28

browser.urlbar
~~~~~~~~~~~~~~

Currently the only documentation for ``browser.urlbar`` is its `schema
<urlbar.json_>`__. Fortunately WebExtension schemas are JSON and aren't too hard
to read. If you need help understanding it, see the `WebExtensions API
implementation documentation <webextAPIImplDoc_>`__.

For examples on using the API, see the Cookbook_ section.

.. _urlbar.json: https://searchfox.org/mozilla-central/source/browser/components/extensions/schemas/urlbar.json

browser.experiments.urlbar
~~~~~~~~~~~~~~~~~~~~~~~~~~

As with ``browser.urlbar``, currently the only documentation for
``browser.experiments.urlbar`` is its schema__. For examples on using the API,
see the Cookbook_ section.

__ https://searchfox.org/mozilla-central/source/browser/components/urlbar/tests/ext/schema.json

Workflow
~~~~~~~~

The web-ext_ command-line tool makes the extension-development workflow very
simple. Simply start it with the *run* command, passing it the location of the
Firefox binary you want to use. web-ext will launch your Firefox and remain
running until you stop it, watching for changes you make to your extension's
files. When it sees a change, it automatically reloads your extension — in
Firefox, in the background — without your having to do anything. It's really
nice.

The `web-ext documentation <web-ext commands_>`__ lists all its options, but
here are some worth calling out for the *run* command:

``--browser-console``
  Automatically open the browser console when Firefox starts. Very useful for
  watching your extension's console logging. (Make sure "Show Content Messages"
  is checked in the console.)

``-p``
  This option lets you specify a path to a profile directory.

``--keep-profile-changes``
  Normally web-ext doesn't save any changes you make to the profile. Use this
  option along with ``-p`` to reuse the same profile again and again.

``--verbose``
  web-ext suppresses Firefox messages in the terminal unless you pass this
  option. If you've added some ``dump`` calls in Firefox because you're working
  on a new ``browser.urlbar`` API, for example, you won't see them without this.

web-ext also has a *build* command that packages your extension's files into a
zip file. The following *build* options are useful:

``--overwrite-dest``
  Without this option, web-ext won't overwrite a zip file it previously created.

web-ext can load its configuration from your extension's package.json. That's
the recommended way to configure it. Here's an example__.

Finally, web-ext can also sign extensions, but if you're developing your
extension for an experiment, you'll use a different process for signing. See
`The Experiment Development Process`_.

.. _web-ext: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Getting_started_with_web-ext
.. _web-ext commands: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/web-ext_command_reference
__ https://github.com/0c0w3/urlbar-top-sites-experiment/blob/6681a7126986bc2565d036b888cb5b8807397ce5/package.json#L7

Automated Tests
~~~~~~~~~~~~~~~

It's possible to write `browser chrome mochitests`_ for your extension the same
way we write tests for Firefox. One of the example extensions linked throughout
this document includes a test_, for instance.

See the readme in the example-addon-experiment_ repo for a workflow.

.. _browser chrome mochitests: https://developer.mozilla.org/en-US/docs/Mozilla/Browser_chrome_tests
.. _test: https://github.com/0c0w3/urlbar-top-sites-experiment/blob/master/tests/tests/browser/browser_urlbarTopSitesExtension.js

Cookbook
~~~~~~~~

*To be written.* For now, you can find example uses of ``browser.experiments.urlbar`` and ``browser.urlbar`` in the following repos:

* https://github.com/mozilla-extensions/firefox-quick-suggest-weather
* https://github.com/0c0w3/urlbar-tips-experiment
* https://github.com/0c0w3/urlbar-top-sites-experiment
* https://github.com/0c0w3/urlbar-search-interventions-experiment

Further Reading
~~~~~~~~~~~~~~~

`WebExtensions on MDN <WebExtensions_>`__
  The place to learn about developing WebExtensions in general.

`Getting started with web-ext <web-ext_>`__
  MDN's tutorial on using web-ext.

`web-ext command reference <web-ext commands_>`__
  MDN's documentation on web-ext's commands and their options.

Developing Address Bar Extension APIs
-------------------------------------

Built-In APIs vs. Experimental APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Originally we developed the address bar extension API in the ``browser.urlbar``
namespace, which is built into Firefox as discussed above. By "built into
Firefox," we mean that the API is developed in `mozilla-central
<urlbar.json_>`__ and shipped inside Firefox just like any other Firefox
feature. At the time, that seemed like the right thing to do because we wanted
to build an API that ultimately could be used by all extension authors, not only
Mozilla.

However, there were a number of disadvantages to this development model. The
biggest was that it tightly coupled our experiments to specific versions of
Firefox. For example, if we were working on an experiment that targeted Firefox
72, then any APIs used by that experiment needed to land and ship in 72. If we
weren't able to finish an API by the time 72 shipped, then the experiment would
have to be postponed until 73. Our experiment development timeframes were always
very short because we always wanted to ship our experiments ASAP. Often we
targeted the Firefox version that was then in Nightly; sometimes we even
targeted the version in Beta. Either way, it meant that we were always uplifting
patch after patch to Beta. This tight coupling between Firefox versions and
experiments erased what should have been a big advantage of implementing
experiments as extensions in the first place: the ability to ship experiments
outside the usual cyclical release process.

Another notable disadvantage of this model was just the cognitive weight of the
idea that we were developing APIs not only for ourselves and our experiments but
potentially for all extensions. This meant that not only did we have to design
APIs to meet our immediate needs, we also had to imagine use cases that could
potentially arise and then design for them as well.

For these reasons, we stopped developing ``browser.urlbar`` and created the
``browser.experiments.urlbar`` experimental API. As discussed earlier,
experimental APIs are APIs that are bundled inside extensions. Experimental APIs
can do anything that built-in APIs can do with the added flexibility of not
being tied to specific versions of Firefox.

Adding New APIs
~~~~~~~~~~~~~~~

All new address bar APIs should be added to ``browser.experiments.urlbar``.
Although this API does not ship in Firefox, it's currently developed in
mozilla-central, in `browser/components/urlbar/tests/ext/ <extDirectory_>`__ --
note the "tests" subdirectory. Developing it in mozilla-central lets us take
advantage of our usual build and testing infrastructure. This way we have API
tests running against each mozilla-central checkin, against all versions of
Firefox that are tested on Mozilla's infrastructure, and we're alerted to any
breaking changes we accidentally make. When we start a new extension repo, we
copy schema.json and api.js to it as described earlier (or clone an example repo
with up-to-date copies of these files).

Generally changes to the API should be reviewed by someone on the address bar
team and someone on the WebExtensions team. Shane (mixedpuppy) is a good
contact.

.. _extDirectory: https://searchfox.org/mozilla-central/source/browser/components/urlbar/tests/ext/

Anatomy of an API
~~~~~~~~~~~~~~~~~

Roughly speaking, a WebExtensions API implementation comprises three different
pieces:

Schema
  The schema declares the functions, properties, events, and types that the API
  makes available to extensions. Schemas are written in JSON.

  The ``browser.experiments.urlbar`` schema is schema.json_, and the
  ``browser.urlbar`` schema is urlbar.json_.

  For reference, the schemas of built-in APIs are in
  `browser/components/extensions/schemas`_ and
  `toolkit/components/extensions/schemas`_.

  .. _browser/components/extensions/schemas: https://searchfox.org/mozilla-central/source/browser/components/extensions/schemas/
  .. _toolkit/components/extensions/schemas: https://searchfox.org/mozilla-central/source/toolkit/components/extensions/schemas/

Internals
  Every API hooks into some internal part of Firefox. For the address bar API,
  that's the Urlbar implementation in `browser/components/urlbar`_.

  .. _browser/components/urlbar: https://searchfox.org/mozilla-central/source/browser/components/urlbar/

Glue
  Finally, there's some glue code that implements everything declared in the
  schema. Essentially, this code mediates between the previous two pieces. It
  translates the function calls, property accesses, and event listener
  registrations made by extensions using the public-facing API into terms that
  the Firefox internals understand, and vice versa.

  For ``browser.experiments.urlbar``, this is api.js_, and for
  ``browser.urlbar``, it's ext-urlbar.js_.

  For reference, the implementations of built-in APIs are in
  `browser/components/extensions`_ and `toolkit/components/extensions`_, in the
  *parent* and *child* subdirecties.  As you might guess, code in *parent* runs
  in the main process, and code in *child* runs in the extensions process.
  Address bar APIs deal with browser chrome and their implementations therefore
  run in the parent process.

  .. _ext-urlbar.js: https://searchfox.org/mozilla-central/source/browser/components/extensions/parent/ext-urlbar.js
  .. _browser/components/extensions: https://searchfox.org/mozilla-central/source/browser/components/extensions/
  .. _toolkit/components/extensions: https://searchfox.org/mozilla-central/source/toolkit/components/extensions/

Keep in mind that extensions run in a different process from the main process.
That has implications for your APIs. They'll generally need to be async, for
example.

Further Reading
~~~~~~~~~~~~~~~

`WebExtensions API implementation documentation <webextAPIImplDoc_>`__
  Detailed info on implementing a WebExtensions API.

.. _webextAPIImplDoc: https://firefox-source-docs.mozilla.org/toolkit/components/extensions/webextensions/

Running Address Bar Extensions
------------------------------

As discussed above, ``browser.experiments.urlbar`` and ``browser.urlbar`` are
privileged APIs. There are two different points to consider when it comes to
running an extension that uses privileged APIs: loading the extension in the
first place, and granting it access to privileged APIs. There's a certain bar
for loading any extension regardless of its API usage that depends on its signed
state and the Firefox build you want to run it in. There's yet a higher bar for
granting it access to privileged APIs. This section discusses how to load
extensions so that they can access privileged APIs.

Since we're interested in extensions primarily for running experiments, there
are three particular signed states relevant to us:

Unsigned
  There are two ways to run unsigned extensions that use privileged APIs.

  They can be loaded temporarily using a Firefox Nightly build or
  Developer Edition but not Beta or Release [source__], and the
  ``extensions.experiments.enabled`` preference must be set to true [source__].
  You can load extensions temporarily by visiting
  about:debugging#/runtime/this-firefox and clicking "Load Temporary Add-on."
  `web-ext <Workflow_>`__ also loads extensions temporarily.

  __ https://searchfox.org/mozilla-central/rev/053826b10f838f77c27507e5efecc96e34718541/toolkit/components/extensions/Extension.jsm#1884
  __ https://searchfox.org/mozilla-central/rev/014fe72eaba26dcf6082fb9bbaf208f97a38594e/toolkit/mozapps/extensions/internal/AddonSettings.jsm#93

  They can be also be loaded normally (not temporarily) in a custom build where
  the build-time setting ``AppConstants.MOZ_REQUIRE_SIGNING`` [source__, source__]
  and ``xpinstall.signatures.required`` pref are both false. As in the previous
  paragraph, such builds include Nightly and Developer Edition but not Beta or
  Release [source__]. In addition, your custom build must modify the
  ``Extension.isPrivileged`` getter__ to return true. This getter determines
  whether an extension can access privileged APIs.

  __ https://searchfox.org/mozilla-central/rev/053826b10f838f77c27507e5efecc96e34718541/toolkit/mozapps/extensions/internal/XPIProvider.jsm#2382
  __ https://searchfox.org/mozilla-central/rev/053826b10f838f77c27507e5efecc96e34718541/toolkit/mozapps/extensions/internal/AddonSettings.jsm#36
  __ https://searchfox.org/mozilla-central/search?q=MOZ_REQUIRE_SIGNING&case=false&regexp=false&path=
  __ https://searchfox.org/mozilla-central/rev/053826b10f838f77c27507e5efecc96e34718541/toolkit/components/extensions/Extension.jsm#1874

  Extensions remain unsigned as you develop them. See the Workflow_ section for
  more.

Signed for testing (Signed for QA)
  Signed-for-testing extensions that use privileged APIs can be run using the
  same techniques for running unsigned extensions.

  They can also be loaded normally (not temporarily) if you use a Firefox build
  where the build-time setting ``AppConstants.MOZ_REQUIRE_SIGNING`` is false and
  you set the ``xpinstall.signatures.dev-root`` pref to true
  [source__]. ``xpinstall.signatures.dev-root`` does not exist by default and
  must be created.

  __ https://searchfox.org/mozilla-central/rev/053826b10f838f77c27507e5efecc96e34718541/toolkit/mozapps/extensions/internal/XPIInstall.jsm#262

  You encounter extensions that are signed for testing when you are writing
  extensions for experiments. See the Experiments_ section for details.

  "Signed for QA" is another way of referring to this signed state.

Signed for release
  Signed-for-release extensions that use privileged APIs can be run in any
  Firefox build with no special requirements.

  You encounter extensions that are signed for release when you are writing
  extensions for experiments. See the Experiments_ section for details.

.. important::
  To see console logs from extensions in the browser console, select the "Show
  Content Messages" option in the console's settings. This is necessary because
  extensions run outside the main process.

Experiments
-----------

**Experiments** let us try out ideas in Firefox outside the usual release cycle
and on particular populations of users.

For example, say we have a hunch that the top sites shown on the new-tab page
aren't very discoverable, so we want to make them more visible. We have one idea
that might work — show them every time the user begins an interaction with the
address bar — but we aren't sure how good an idea it is. So we test it. We write
an extension that does just that, make sure it collects telemetry that will help
us answer our question, ship it outside the usual release cycle to a small
percentage of Beta users, collect and analyze the telemetry, and determine
whether the experiment was successful. If it was, then we might want to ship the
feature to all Firefox users.

Experiments sometimes are also called **studies** (not to be confused with *user
studies*, which are face-to-face interviews with users conducted by user
researchers).

There are two types of experiments:

Pref-flip experiments
  Pref-flip experiments are simple. If we have a fully baked feature in the
  browser that's preffed off, a pref-flip experiment just flips the pref on,
  enabling the feature for users running the experiment. No code is required.
  We tell the experiments team the name of the pref we want to flip, and they
  handle it.

  One important caveat to pref-flip studies is that they're currently capable of
  flipping only a single pref. There's an extension called Multipreffer_ that
  can flip multiple prefs, though.

  .. _Multipreffer: https://github.com/mozilla/multipreffer

Add-on experiments
  Add-on experiments are much more complex but much more powerful. (Here
  *add-on* is a synonym for extension.) They're the type of experiments that
  this document has been discussing all along.

  An add-on experiment is shipped as an extension that we write and that
  implements the experimental feature we want to test. To reiterate, the
  extension is a WebExtension and uses WebExtensions APIs. If the current
  WebExtensions APIs do not meet the needs of your experiment, then you must
  create either experimental or built-in APIs so that your extension can use
  them. If necessary, you can make any new built-in APIs privileged so that they
  are available only to Mozilla extensions.

  An add-on experiment can collect additional telemetry that's not collected in
  the product by using the privileged ``browser.telemetry`` WebExtensions API,
  and of course the product will continue to collect all the telemetry it
  usually does. The telemetry pings from users running the experiment will be
  correlated with the experiment with no extra work on our part.

A single experiment can deliver different UXes to different groups of users
running the experiment. Each group or UX within an experiment is called a
**branch**. Experiments often have two branches, control and treatment. The
**control branch** actually makes no UX changes. It may capture additional
telemetry, though. Think of it as the control in a science experiment. It's
there so we can compare it to data from the **treatment branch**, which does
make UX changes. Some experiments may require multiple treatment branches, in
which case the different branches will have different names. Add-on experiments
can implement all branches in the same extension or each branch in its own
extension.

Experiments are delivered to users by a system called **Normandy**. Normandy
comprises a client side that lives in Firefox and a server side. In Normandy,
experiments are defined server-side in files called **recipes**. Recipes include
information about the experiment like the Firefox release channel and version
that the experiment targets, the number of users to be included in the
experiment, the branches in the experiment, the percentage of users on each
branch, and so on.

Experiments are tracked by Mozilla project management using a system called
Experimenter_.

Finally, there was an older version of the experiments program called
**Shield**. Experiments under this system were called **Shield studies** and
could be be shipped as extensions too.

.. _Experimenter: https://experimenter.services.mozilla.com/

Further Reading
~~~~~~~~~~~~~~~

`Pref-Flip and Add-On Experiments <https://mana.mozilla.org/wiki/pages/viewpage.action?spaceKey=FIREFOX&title=Pref-Flip+and+Add-On+Experiments>`__
  A comprehensive document on experiments from the Experimenter team. See the
  child pages in the sidebar, too.

`Client Implementation Guidelines for Experiments <https://docs.telemetry.mozilla.org/cookbooks/client_guidelines.html>`_
  Relevant documentation from the telemetry team.

#ask-experimenter Slack channel
  A friendly place to get answers to your experiment questions.

The Experiment Development Process
----------------------------------

This section describes an experiment's life cycle.

1. Experiments usually originate with product management and UX. They're
   responsible for identifying a problem, deciding how an experiment should
   approach it, the questions we want to answer, the data we need to answer
   those questions, the user population that should be enrolled in the
   experiment, the definition of success, and so on.

2. UX makes a spec that describes what the extension looks like and how it
   behaves.

3. There's a kickoff meeting among the team to introduce the experiment and UX
   spec. It's an opportunity for engineering to ask questions of management, UX,
   and data science. It's really important for engineering to get a precise and
   accurate understanding of how the extension is supposed to behave — right
   down to the UI changes — so that no one makes erroneous assumptions during
   development.

4. At some point around this time, the team (usually management) creates a few
   artifacts to track the work and facilitate communication with outside teams
   involved in shipping experiments. They include:

   * A page on `Experimenter <Experiments_>`__
   * A QA PI (product integrity) request so that QA resources are allocated
   * A bug in `Data Science :: Experiment Collaboration`__ so that data science
     can track the work and discuss telemetry (engineering might file this one)

   __ https://bugzilla.mozilla.org/enter_bug.cgi?assigned_to=nobody%40mozilla.org&bug_ignored=0&bug_severity=normal&bug_status=NEW&bug_type=task&cf_firefox_messaging_system=---&cf_fx_iteration=---&cf_fx_points=---&comment=%23%23%20Brief%20Description%20of%20the%20request%20%28required%29%3A%0D%0A%0D%0A%23%23%20Business%20purpose%20for%20this%20request%20%28required%29%3A%0D%0A%0D%0A%23%23%20Requested%20timelines%20for%20the%20request%20or%20how%20this%20fits%20into%20roadmaps%20or%20critical%20decisions%20%28required%29%3A%0D%0A%0D%0A%23%23%20Links%20to%20any%20assets%20%28e.g%20Start%20of%20a%20PHD%2C%20BRD%3B%20any%20document%20that%20helps%20describe%20the%20project%29%3A%0D%0A%0D%0A%23%23%20Name%20of%20Data%20Scientist%20%28If%20Applicable%29%3A%0D%0A%0D%0A%2APlease%20note%20if%20it%20is%20found%20that%20not%20enough%20information%20has%20been%20given%20this%20will%20delay%20the%20triage%20of%20this%20request.%2A&component=Experiment%20Collaboration&contenttypemethod=list&contenttypeselection=text%2Fplain&filed_via=standard_form&flag_type-4=X&flag_type-607=X&flag_type-800=X&flag_type-803=X&flag_type-936=X&form_name=enter_bug&maketemplate=Remember%20values%20as%20bookmarkable%20template&op_sys=Unspecified&priority=--&product=Data%20Science&rep_platform=Unspecified&target_milestone=---&version=unspecified

5. Engineering breaks down the work and files bugs. There's another engineering
   meeting to discuss the breakdown, or it's discussed asynchronously.

6. Engineering sets up a GitHub repo for the extension. See `Implementing
   Experiments`_ for an example repo you can clone to get started. Disable
   GitHub Issues on the repo so that QA will file bugs in Bugzilla instead of
   GitHub. There's nothing wrong with GitHub Issues, but our team's project
   management tracks all work through Bugzilla. If it's not there, it's not
   captured.

7. Engineering or management fills out the Add-on section of the Experimenter
   page as much as possible at this point. "Active Experiment Name" isn't
   necessary, and "Signed Release URL" won't be available until the end of the
   process.

8. Engineering implements the extension and any new WebExtensions APIs it
   requires.

9. When the extension is done, engineering or management clicks the "Ready for
   Sign-Off" button on the Experimenter page. That changes the page's status
   from "Draft" to "Ready for Sign-Off," which allows QA and other teams to sign
   off on their portions of the experiment.

10. Engineering requests the extension be signed "for testing" (or "for
    QA"). Michael (mythmon) from the Experiments team and Rehan (rdalal) from
    Services Engineering are good contacts. Build the extension zip file using
    web-ext as discussed in Workflow_. Attach it to a bug (a metabug for
    implementing the extension, for example), needinfo Michael or Rehan, and ask
    him to sign it. He'll attach the signed version to the bug. If neither
    Michael nor Rehan is available, try asking in the #ask-experimenter Slack
    channel.

11. Engineering sends QA the link to the signed extension and works with them to
    resolve bugs they find.

12. When QA signs off, engineering asks Michael to sign the extension "for
    release" using the same needinfo process described earlier.

13. Paste the URL of the signed extension in the "Signed Release URL" textbox of
    the Add-on section of the Experimenter page.

14. Other teams sign off as they're ready.

15. The experiment ships! 🎉


Implementing Experiments
------------------------

This section discusses how to implement add-on experiments. Pref-flip
experiments are much simpler and don't need a lot of explanation. You should be
familiar with the concepts discussed in the `Developing Address Bar Extensions`_
and `Running Address Bar Extensions`_ sections before reading this one.

The most salient thing about add-on experiments is that they're implemented
simply as privileged extensions. Other than being privileged and possibly
containing bundled experimental APIs, they're similar to all other extensions.

The `top-sites experiment extension <topSites_>`__ is an example of a real,
shipped experiment.

.. _topSites: https://github.com/0c0w3/urlbar-top-sites-experiment

Setup
~~~~~

example-addon-experiment_ is a repo you can clone to get started. It's geared
toward urlbar extensions and includes the stub of a browser chrome mochitest.

.. _example-addon-experiment: https://github.com/0c0w3/example-addon-experiment

browser.normandyAddonStudy
~~~~~~~~~~~~~~~~~~~~~~~~~~

As discussed in Experiments_, an experiment typically has more than one branch
so that it can test different UXes. The experiment's extension(s) needs to know
the branch the user is enrolled in so that it can behave appropriately for the
branch: show the user the proper UX, collect the proper telemetry, and so on.

This is the purpose of the ``browser.normandyAddonStudy`` WebExtensions API.
Like ``browser.urlbar``, it's a privileged API available only to Mozilla
extensions.

Its schema is normandyAddonStudy.json_.

It's a very simple API. The primary function is ``getStudy``, which returns the
study the user is currently enrolled in or null if there isn't one. (Recall that
*study* is a synonym for *experiment*.) One of the first things an experiment
extension typically does is to call this function.

The Normandy client in Firefox will keep an experiment extension installed only
while the experiment is active. Therefore, ``getStudy`` should always return a
non-null study object. Nevertheless, the study object has an ``active`` boolean
property that's trivial to sanity check. (The example extension does.)

The more important property is ``branch``, the name of the branch that the user
is enrolled in. Your extension should use it to determine the appropriate UX.

Finally, there's an ``onUnenroll`` event that's fired when the user is
unenrolled in the study. It's not quite clear in what cases an extension would
need to listen for this event given that Normandy automatically uninstalls
extensions on unenrollment. Maybe if they create some persistent state that's
not automatically undone on uninstall by the WebExtensions framework?

If your extension itself needs to unenroll the user for some reason, call
``endStudy``.

.. _normandyAddonStudy.json: https://searchfox.org/mozilla-central/source/browser/components/extensions/schemas/normandyAddonStudy.json

Telemetry
~~~~~~~~~

Experiments can capture telemetry in two places: in the product itself and
through the privileged ``browser.telemetry`` WebExtensions API. The API schema
is telemetry.json_.

The telemetry pings from users running experiments are automatically correlated
with those experiments, no extra work required. That's true regardless of
whether the telemetry is captured in the product or though
``browser.telemetry``.

The address bar has some in-product, preffed off telemetry that we want to
enable for all our experiments — at least that's the thinking as of August 2019.
It's called `engagement event telemetry`_, and it records user *engagements*
with and *abandonments* of the address bar [source__]. We added a
BrowserSetting_ on ``browser.urlbar`` just to let us flip the pref and enable
this telemetry in our experiment extensions. Call it like this::

    await browser.urlbar.engagementTelemetry.set({ value: true });

.. _telemetry.json: https://searchfox.org/mozilla-central/source/toolkit/components/extensions/schemas/telemetry.json
.. _engagement event telemetry: https://bugzilla.mozilla.org/show_bug.cgi?id=1559136
__ https://searchfox.org/mozilla-central/rev/7088fc958db5935eba24b413b1f16d6ab7bd13ea/browser/components/urlbar/UrlbarController.jsm#598
.. _BrowserSetting: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/types/BrowserSetting

Engineering Best Practices
~~~~~~~~~~~~~~~~~~~~~~~~~~

Clear up questions with your UX person early and often. There's often a gap
between what they have in their mind and what you have in yours.  Nothing wrong
with that, it's just the nature of development. But misunderstandings can cause
big problems when they're discovered late. This is especially true of UX
behaviors, as opposed to visuals or styling. It's no fun to realize at the end
of a release cycle that you've designed the wrong WebExtensions API because some
UX detail was overlooked.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Related to the previous point, make builds of your extension for your UX person
so they can test it.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Taking the previous point even further, if your experiment will require a
substantial new API(s), you might think about prototyping the experiment
entirely in a custom Firefox build before designing the API at all. Give it to
your UX person. Let them disect it and tell you all the problems with it. Fill
in all the gaps in your understanding, and then design the API. We've never
actually done this, though.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It's a good idea to work on the extension as you're designing and developing the
APIs it'll use. You might even go as far as writing the first draft of the
extension before even starting to implement the APIs. That lets you spot
problems that may not be obvious were you to design the API in isolation.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Your extension's ID should end in ``@shield.mozilla.org``. QA will flag it if it
doesn't.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set ``"hidden": true`` in your extension's manifest.json. That hides it on
about:addons. (It can still be seen on about:studies.) QA will spot this if you
don't.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are drawbacks of hiding features behind prefs and enabling them in
experiment extensions. Consider not doing that if feasible, or at least weigh
these drawbacks against your expected benefits.

* Prefs stay flipped on in private windows, but experiments often have special
  requirements around private-browsing mode (PBM). Usually, they shouldn't be
  active in PBM at all, unless of course the point of the experiment is to test
  PBM. Extensions also must request PBM access ("incognito" in WebExtensions
  terms), and the user can disable access at any time. The result is that part
  of your experiment could remain enabled — the part behind the pref — while
  other parts are disabled.

* Prefs stay flipped on in safe mode, even though your extension (like all
  extensions) will be disabled. This might be a bug__ in the WebExtensions
  framework, though.

  __ https://bugzilla.mozilla.org/show_bug.cgi?id=1576997
