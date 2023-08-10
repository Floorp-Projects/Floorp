Gecko
=====

Gecko is Mozilla's rendering engine for the web. It is made up of HTML parsing and rendering,
networking, JavaScript, IPC, DOM, OS widget abstractions and much much more. It also includes some
UI components that are shared with applications built on top of Gecko such as Firefox for Desktop,
Firefox for Android, and Thunderbird. As well as rendering web pages Gecko is also responsible for
rendering the application's UI in some applications.

Networking (necko)
------------------

The networking engine services requests for DNS queries as well as for content hosted on web servers
using either http, http/2 or http/3 protocols to retrieve it. Necko uses NSS
(`Network Security Services library <https://wiki.mozilla.org/NSS>`_) for its cryptographic uses
e.g. to perform secure requests using TLS.

:ref:`Read more <Networking>`

JavaScript (SpiderMonkey)
-------------------------

The JavaScript engine is responsible for running JavaScript code both in content processes for
webpages as well as the JavaScript code that makes up the bulk of the UI in applications like
Firefox and Thunderbird.

:ref:`Read more <SpiderMonkey>`

JavaScript modules
##################

SpiderMonkey supports a proprietary type of JavaScript modules that was developed before the
EcmaScript module standard and even before commonjs was popular. These modules define exports using
an EXPORTED_SYMBOLS array containing a list of symbol names to be exported. This kind of module is
being replaced with standard EcmaScript modules.

XPCOM
-----

XPCOM (Cross-Platform Component Object Model) is Mozilla's version of Microsoft's
`COM <https://en.wikipedia.org/wiki/Component_Object_Model>`_.

XPCOM and :ref:`WebIDL <WebIDL>` are the primary ways for our frontend to communicate with the
underlying platform and to invoke methods that are implemented in native code.

XPCOM performs the following critical functions:

#. Allows creating software components with strictly defined
   `interfaces <https://searchfox.org/mozilla-central/search?q=&path=.idl&case=false&regexp=false>`_
   using :ref:`XPIDL <XPIDL>`. These components can be implemented in C++, JavaScript or Rust. They
   can also be invoked and manipulated in any of those languages regardless of the underlying
   implementation language.
#. Acts as a process-global registry of named components (there are singleton "services" as well as
   factories for creating instances of components).
#. Allows components and services to implement multiple interfaces, and to be dynamically cast to
   those interfaces using ``QueryInterface``.

If that all sounds rather abstract, that's because it is. XPCOM is one of the oldest Mozilla
technologies that Firefox is still built on top of. XPCOM made a lot more sense in the late 90s when
Microsoft COM was still popular and the Mozilla codebase was also being developed as a general
application development platform for third-parties. There have been
`long-standing efforts <https://bugzilla.mozilla.org/show_bug.cgi?id=decom>`_ to move away from or
simplify XPCOM in places where its usefulness is questionable.

.. mermaid::

     sequenceDiagram
         Caller->>Component Registry: Get service @mozilla.org/cookie-banner-service#59;1
         Component Registry->>nsCookieBannerService: new
         nsCookieBannerService-->>Component Registry: return
         Component Registry-->>Caller: return
         Caller->>nsCookieBannerService: QueryInterface(nsICookieBannerService)
         nsCookieBannerService-->>Caller: return


:ref:`Read more <XPCOM>`

Process Separation / Fission / IPC / Actors
-------------------------------------------

Firefox is a multi-process application. Over the lifetime of the main Firefox process, many other
sub processes can be started and stopped. A full catalogue of those different processes can be found
:ref:`here <Process Model>`.

Firefox communicates between these processes (mostly) asynchronously using the native inter-process
communication mechanisms of the underlying platform. Those mechanisms and their details are hidden
under cross-platform abstractions like :ref:`IPDL <IPDL: Inter-Thread and Inter-Process Message Passing>`
(for native code) and :ref:`JSActors <JSActors>` (for frontend code).

Firefox’s initial web content process separation (this was Project "Electrolysis", sometimes
shortened to “e10s”) shipped in 2016, and separated all web content into a single shared content
process. Not long after that, multiple content processes were enabled, and the web content of tabs
would be assigned to one of the created content processes using a round-robin scheme. In 2021, as
part of the mitigations for the `Spectre <https://en.wikipedia.org/wiki/Spectre_(security_vulnerability)>`_
and `Meltdown <https://en.wikipedia.org/wiki/Meltdown_(security_vulnerability)>`_ processor
vulnerabilities, Firefox’s process model changed to enforce a model where each content process only
loads and executes instructions from a single site (this was Project “Fission”). You can read more
about the `underlying rationale and technical details about Project Fission <https://hacks.mozilla.org/2021/05/introducing-firefox-new-site-isolation-security-architecture/>`_.

DOM + WebIDL
------------

The :ref:`DOM APIs <DOM>` implement the functionality of elements in webpages and UI that is
rendered by Gecko.

:ref:`WebIDL <WebIDL>` is a standard specification for describing the interfaces to DOM objects. As
well as defining the interface for webpages Gecko also makes use of it for defining the interface to
various internal components. Like XPCOM, components that implement WebIDL interfaces can be called
from both C++ and JavaScript.

Style System (CSS)
------------------

The style system is responsible for parsing the document's CSS and using that to resolve a value for
every CSS property on every element in the document.  This determines many characteristics of how
each element will render (e.g. fonts, colors, size, layout model).

:ref:`Read more <Layout & CSS>`

Layout
------

The layout engine is responsible for taking the DOM and styles and generating and updating a frame
tree ready for presentation to the user.

:ref:`Read more <Layout & CSS>`

Graphics
--------

The graphics component is responsible for taking the frame tree generated by the layout engine
and presenting it on screen.

:ref:`Read more <Graphics>`

Localization (Fluent)
---------------------

At Mozilla, localizations are managed by locale communities around the world, who are responsible
for maintaining high quality linguistic and cultural adaptation of Mozilla software into over 100
locales.

The exact process of localization management differs from project to project, but in the case of
Gecko applications, the localization is primarily done via a web localization system called
`Pontoon <https://pontoon.mozilla.org/>`_ and stored in HG repositories under
`hg.mozilla.org/l10n-central <https://hg.mozilla.org/l10n-central/>`_.

:ref:`Read more <Localization>`

Profiles
--------

A user profile is where Gecko stores settings, caches and any other data that must persist after the
application exits. It is made up of two directories on disk. The root directory (often just called
the profile directory) is where settings are stored. The local directory is for caches or any other
data that is temporary and will be rebuilt with no perceived loss to the user should it be
unavailable. These two directories can just be the same directory on disk. In an enterprise
environment or other situation where a user often switches between computers the root directory is
intended to be in a location on the network accessible to all computers while the local directory
can be local to the computer.

The profile service maintains a database of named user profiles that can be selected either from the
command line or through a basic user interface. Additionally command line arguments exist that will
run an application using any given directory for the user profile.

Preferences
-----------

The preferences service is a basic key value store for a user's settings. The keys are simple
strings and although are often considered to be hierarchical with parts separated by periods
internally everything is just held as flat lists. Preference values can be strings, integers or
boolean.

:ref:`Read more <libpref>`

Observer Service
----------------

The Observer Service (nsIObserverService) is a process-global XPCOM service that acts as a general
message bus implementing the `publish-subscribe pattern <https://en.wikipedia.org/wiki/Publish%E2%80%93subscribe_pattern>`_.
Components implementing nsIObserver (or simple functions in JavaScript) can be registered with the
observer service to be notified when particular "topics" (topics are just developer-defined strings)
have occurred. This is particularly useful for creating a dependency between two components without
tightly coupling them.

For example, suppose there is a mechanism that clears a user's browsing history from the disk and
memory. At the end of that process, it might tell the observer service to notify on a topic like
"browser-clear-history". An observer registered for that topic might use that signal to know to
clear some of its caches, which might also contain browsing history.

Principals / Security model
---------------------------

Whenever Firefox on Desktop or Android fetches a resource from the web, Firefox performs a variety
of web security checks. Most prominently the `Same-origin Policy <https://developer.mozilla.org/en-US/docs/Web/Security/Same-origin_policy>`_
to ensure web pages can not harm end users by performing malicious actions, like e.g. accessing the
local file system. All web related security checks within Firefox are evaluated based on the
security concept of a Principal, which slightly simplified represents an origin. More precisely,
Firefox captures the security context using one of the following four types of Principals:

* Content-Principal, which reflects the Security Context of web content (origin). For example, when
  visiting https://example.com a Content-Principal of https://example.com reflects the security
  context of that origin and passes if scheme, host and port match.
* Null-Principal, which reflects a sandboxed (or least privilege) Security Context. For example,
  when loading an iframe with a sandbox attribute Firefox internally generates a Null-Principal to
  reflect that security context. A Null-Principal is only same-origin with itself.
* System-Principal, which reflects the security context of browser chrome-code and passes all
  security checks. Important: Never use SystemPrincipal if the URI to be loaded can be influenced by
  web content.
* Expanded-Principal, which is a list of principals to match the security needs for Content Scripts
  in Firefox Extensions.

Whenever Firefox starts to load a resource (e.g. script, css, image) then security relevant meta
information including `nsIPrincipal <https://searchfox.org/mozilla-central/source/caps/nsIPrincipal.idl>`_
is attached to the `nsILoadInfo <https://searchfox.org/mozilla-central/source/netwerk/base/nsILoadInfo.idl>`_.
This load context providing object remains attached to the resource load (
`nsIChannel <https://searchfox.org/mozilla-central/source/netwerk/base/nsIChannel.idl>`_) throughout
the entire loading life cycle of a resource and allows Firefox to provide the same security
guarantees even if the resource load encounters a server side redirect.

Please find all the details about the Security Model of Firefox by reading the blog posts:
Understanding Web Security Checks in Firefox (
`Part 1 <https://blog.mozilla.org/attack-and-defense/2020/06/10/understanding-web-security-checks-in-firefox-part-1/>`_ &
`Part 2 <https://blog.mozilla.org/attack-and-defense/2020/08/05/understanding-web-security-checks-in-firefox-part-2/>`_)
and `Enforcing Content Security By Default within Firefox <https://blog.mozilla.org/security/2016/11/10/enforcing-content-security-by-default-within-firefox/>`_.

Chrome Protocol
---------------

The chrome protocol is an internal protocol used to reference files that ship as part of the
application. It is of the form ``chrome://<package>/<provider>/…`` where provider is one of content,
skin or locale. The majority of files referenced by the chrome protocol are stored in the omni.ja
files which are generated from :ref:`JAR manifest files <JAR Manifests>` at build time.
:ref:`Chrome manifest files <Chrome Registration>` are used to register where in the jar files
different packages are stored.

Resource Protocol
-----------------

The resource protocol is another internal protocol that can reference files that ship as part of the
application. Strictly speaking it is simply a mapped, all urls of the form ``resource://<package>/…``
are mapped to ``<new-uri>/…``. The mappings are generally defined using the resource instruction in
:ref:`chrome manifest files <chrome_manifest_resource>` however can also be defined at runtime and
some hardcoded mappings. Common examples include:

* ``resource://gre/…`` which references files in the gecko omni.ja file.
* ``resource://app/…``, often simplified as ``resource:///…`` which references files in the application
  omni.ja file.

About pages/protocol
--------------------

The ``about`` protocol allows for binding short human-readable urls to internal content to be
displayed in the content area. For the most part each about page is simply a simpler name for
content in the chrome or resource protocols. For example the page ``about:processes`` simply loads
``chrome://global/content/aboutProcesses.html``. About pages are registered in the
`global <https://searchfox.org/mozilla-central/source/docshell/base/nsAboutRedirector.cpp>`_ and
`desktop <https://searchfox.org/mozilla-central/source/browser/components/about/AboutRedirector.cpp>`_
redirector components.

Toolkit
-------

Toolkit consists of components that can be shared across multiple applications built on top of
Gecko. For example, much of our WebExtensions API surfaces are implemented in toolkit, as several of
these APIs are shared between both Firefox, Firefox for Android, and in some cases Thunderbird.

:ref:`Read more <Toolkit>`

Linting / building / testing / developer workflow
-------------------------------------------------

Set-up the build environment using the :ref:`contributor's quick reference <Firefox Contributors' Quick Reference>`.

Make yourself aware of the :ref:`Linting set-up <Linting>`, in particular how to run
:ref:`linters and add hooks to automatically run the linters on commit <Running Linters Locally>`.
Additionally, make sure you set-up your editor with appropriate settings for linters. For VS Code,
these are set up automatically, as :ref:`per the documentation <Visual Studio Code>`.

For front-end work, ESLint and Prettier are the linters you'll use the most, see the
:ref:`section on ESLint <ESLint>` for details of both of those, which also has
:ref:`an FAQ <eslint_common_issues>`.

Details about :ref:`automated tests may be found here <Automated Testing>`. The most commonly used
tests are :ref:`XPCShell <XPCShell tests>` for testing backend components,
:ref:`Browser Chrome Tests <Browser chrome mochitests>` for testing the frontend UI and
:ref:`Web Platform Tests <web-platform-tests>` for testing web APIs.

WebExtensions
--------------

The WebExtensions APIs allow extensions to interact with the rest of the browser.

:ref:`Read more <WebExtensions API Development>`
