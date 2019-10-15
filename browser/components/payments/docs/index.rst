==============
WebPayments UI
==============

User Interface for the WebPayments `Payment Request API <https://w3c.github.io/browser-payment-api/>`_ and `Payment Handler API <https://w3c.github.io/payment-handler/>`_.


  `Project Wiki <https://wiki.mozilla.org/Firefox/Features/Web_Payments>`_ |
  `#payments on IRC <ircs://irc.mozilla.org:6697/payments>`_ |
  `File a bug <https://bugzilla.mozilla.org/enter_bug.cgi?product=Firefox&component=WebPayments%20UI&status_whiteboard=[webpayments]%20[triage]>`_

JSDoc style comments are used within the JS files of the component. This document will focus on higher-level and shared concepts.

.. toctree::
   :maxdepth: 5


Debugging/Development
=====================

Relevant preferences: ``dom.payments.*``

Must Have Electrolysis
----------------------

Web Payments `does not work without e10s <https://bugzilla.mozilla.org/show_bug.cgi?id=1365964>`_!

Logging
-------

Set the pref ``dom.payments.loglevel`` to "Debug" to increase the verbosity of console messages.

Unprivileged UI Development
---------------------------
During development of the unprivileged custom elements, you can load the dialog in a tab with
the url `resource://payments/paymentRequest.xhtml`.
You can then use the debugging console to load sample data. Autofill add/edit form strings
will not appear when developing this way until they are converted to FTL.
You can force localization of Form Autofill strings using the following in the Browser Console when
the `paymentRequest.xhtml` tab is selected then reloading::

    gBrowser.selectedBrowser.messageManager.loadFrameScript("chrome://formautofill/content/l10n.js", true)


Debugging Console
-----------------

To open the debugging console in the dialog, use the keyboard shortcut
**Ctrl-Alt-d (Ctrl-Option-d on macOS)**. While loading `paymentRequest.xhtml` directly in the
browser, add `?debug=1` to have the debugging console open by default.

Debugging the unprivileged frame with the developer tools
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To open a debugger in the context of the remote payment frame, click the "Debug frame" button in the
debugging console.

Use the ``tabs`` variable in the Browser Content Toolbox's console to access the frame contents.
There can be multiple frames loaded in the same process so you will need to find the correct tab
in the array by checking the file name is `paymentRequest.xhtml` (e.g. ``tabs[0].content.location``).


Dialog Architecture
===================

Privileged wrapper XUL document (paymentDialogWrapper.xul) containing a remote ``<xul:browser="true" remote="true">`` containing unprivileged XHTML (paymentRequest.xhtml).
Keeping the dialog contents unprivileged is useful since the dialog will render payment line items and shipping options that are provided by web developers and should therefore be considered untrusted.
In order to communicate across the process boundary a privileged frame script (`paymentDialogFrameScript.js`) is loaded into the iframe to relay messages.
This is because the unprivileged document cannot access message managers.
Instead, all communication across the privileged/unprivileged boundary is done via custom DOM events:

* A ``paymentContentToChrome`` event is dispatched when the dialog contents want to communicate with the privileged dialog wrapper.
* A ``paymentChromeToContent`` event is dispatched on the ``window`` with the ``detail`` property populated when the privileged dialog wrapper communicates with the unprivileged dialog.

These events are converted to/from message manager messages of the same name to communicate to the other process.
The purpose of `paymentDialogFrameScript.js` is to simply convert unprivileged DOM events to/from messages from the other process.

The dialog depends on the add/edit forms and storage from :doc:`Form Autofill </browser/extensions/formautofill/docs/index>` for addresses and credit cards.

Communication with the DOM
--------------------------

Communication from the DOM to the UI happens via the `paymentUIService.js` (implementing ``nsIPaymentUIService``).
The UI talks to the DOM code via the ``nsIPaymentRequestService`` interface.


Custom Elements
---------------

The Payment Request UI uses `Custom Elements <https://developer.mozilla.org/en-US/docs/Web/Web_Components/Using_custom_elements>`_ for the UI components.

Some guidelines:

* There are some `mixins <https://dxr.mozilla.org/mozilla-central/source/browser/components/payments/res/mixins/>`_
  to provide commonly needed functionality to a custom element.
* `res/containers/ <https://dxr.mozilla.org/mozilla-central/source/browser/components/payments/res/containers/>`_
  contains elements that react to application state changes,
  `res/components/ <https://dxr.mozilla.org/mozilla-central/source/browser/components/payments/res/components>`_
  contains elements that aren't connected to the state directly.
* Elements should avoid having their own internal/private state and should react to state changes.
  Containers primarily use the application state (``requestStore``) while components primarily use attributes.
* If you're overriding a lifecycle callback, don't forget to call that method on
  ``super`` from the implementation to ensure that mixins and ancestor classes
  work properly.
* From within a custom element, don't use ``document.getElementById`` or
  ``document.querySelector*`` because they can return elements that are outside
  of the component, thus breaking the modularization. It can also cause problems
  if the elements you're looking for aren't attached to the document yet. Use
  ``querySelector*`` on ``this`` (the custom element) or one of its descendants
  instead.
