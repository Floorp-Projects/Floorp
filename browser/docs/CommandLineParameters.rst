=======================
Command Line Parameters
=======================

Firefox recognizes many (many!) command line parameters.  Overview
documentation of these parameters lives here.

Known parameters
----------------

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Parameter
     - Description
   * - ``-osint``
     - On Windows, ``-osint`` serves two purposes.  Most importantly, it signals
       that the command line is untrusted and must be sanitized.  Command lines
       with ``-osint`` are rejected unless they have a very specific structure,
       usually ``firefox.exe -osint -url URL`` or ``firefox.exe -osint
       -private-window URL``: refer to `the EnsureCommandLineSafe function
       <https://searchfox.org/mozilla-central/rev/ead7da2d9c5400bc7034ff3f06a030531bd7e5b9/toolkit/xre/CmdLineAndEnvUtils.h#196>`_.
       These command lines are produced by apps delegating to Firefox, and the
       resulting URL may not be correctly quoted.  The sanitization process
       ensures that maliciously chosen URLs do not add additional parameters to
       Firefox.  Secondarily, the ``-osint`` parameter signals that Firefox is
       being invoked by Windows to handle a URL: generally a registered file
       type, e.g., ``.html``, or protocol, e.g., ``https``.
