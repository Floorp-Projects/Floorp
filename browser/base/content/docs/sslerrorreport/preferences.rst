.. _healthreport_preferences:

===========
Preferences
===========

The following preferences are used by SSL Error reporting:

"security.ssl.errorReporting.enabled"
  Should the SSL Error Reporting UI be shown on pin violations? Default
  value: ``true``

"security.ssl.errorReporting.url"
  Where should SSL error reports be sent? Default value:
  ``https://incoming.telemetry.mozilla.org/submit/sslreports/``

"security.ssl.errorReporting.automatic"
  Should error reports be sent without user interaction. Default value:
  ``false``. Note: this pref is overridden by the value of
  ``security.ssl.errorReporting.enabled``
  This is only set when specifically requested by the user. The user can set
  this value (or unset it) by checking the "Automatically report errors in the
  future" checkbox when about:neterror is displayed for SSL Errors.
