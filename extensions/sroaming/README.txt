This implements 4.x-like roaming.

To make the implementation vastly more simple, it has been decided that no 
syncing during the session happens. The design will not allow that either 
(at most sync in certain intervalls). A full-blown dynamic implementation 
that immediately update the server when a data change occured requires the 
cooperation of the data providers (bookmarks, prefs etc.) and is thus a huge 
change that I will leave to somebody else to implement independant of this 
roaming support here. alecf made such proposals a longer time ago on 
n.p.m.prefs, they sounded very interesting, but unfortunately, nobody 
implemented them so far.

When the users selected a profile, we will check, if it's a roaming profile 
and where the data lies. If necessary, we will contact the server and 
download the data as files. We will overwrite local profile files with 
the downloaded ones. Then, the profile works as if it were fully local. 
When the user then logs out (shuts down Mozilla or switches to another 
profile), we upload the local files, overwriting those on the server.

Following Conrad Carlen's advise, I do not hook up using nsIProfileChangeStatus, 
but in nsProfile directly. That just calls |nsISessionRoaming|. 
Its implementation uses various protocol handlers like |mozSRoamingCopy| to 
do the upload/download. These in turn may use generic protocol handlers like 
the netwerk HTTP protocol to do that.

Also following Conrad's advise, I do not store the roaming prefs in the prefs 
system (prefs.js etc.), because that it not yet initialized when I need the 
data (of course - prefs.js, user.js etc. might get changed by us), but in 
the Mozilla application registry. For the structure, see the comment at 
the top of prefs/top.js.


Overview of implementation:
- transfer.js (the Transfer class and support classes) contains the
  non-GUI logic to transfer files and track the progress and success.
- progressDialog.* shows the progress to the user (and also works as interface
  to the C++ code)
- conflictCheck.js is the "controller", controls the overall execution flow.
  It determines what has to be done (which files to transfer when etc.),
  including the conflict resolution logic, and kicks off the transfers.
  There is a long comment at the top describing the implementation.
- conflictResolution.* is a dialog to ask the user when we don't know which
  version of a file to use.
