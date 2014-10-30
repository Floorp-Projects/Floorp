.. _healthreport_dataformat:

==============
Payload Format
==============

An example report::

  {
    "timestamp":1413490449,
    "errorCode":-16384,
    "failedCertChain":[
      ],
    "userAgent":"Mozilla/5.0 (X11; Linux x86_64; rv:36.0) Gecko/20100101 Firefox/36.0",
    "version":1,
    "build":"20141022164419",
    "product":"Firefox",
    "channel":"default"
  }

Where the data represents the following:

"timestamp"
  The (local) time at which the report was generated. Seconds since 1 Jan 1970,
  UTC.

"errorCode"
  The error code. This is the error code from certificate verification. Here's a small list of the most commonly-encountered errors:
  https://wiki.mozilla.org/SecurityEngineering/x509Certs#Error_Codes_in_Firefox
  In theory many of the errors from sslerr.h, secerr.h, and pkixnss.h could be encountered. We're starting with just MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE, which means that key pinning failed (i.e. there wasn't an intersection between the keys in any computed trusted certificate chain and the expected list of keys for the domain the user is attempting to connect to).

"failedCertChain"
  The certificate chain which caused the pinning violation (array of base64
  encoded PEM)

"user agent"
  The user agent string of the browser sending the report

"build"
  The build ID

"product"
  The product name

"channel"
  The user's release channel
