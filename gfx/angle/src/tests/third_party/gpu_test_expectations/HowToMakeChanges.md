Because the ```gpu_test_expectations``` directory is based on parts of Chromium's ```gpu/config``
directory, we want to keep a patch of the changes added to make it compile with ANGLE. This
will allow us to merge Chromium changes easily in our ```gpu_test_expectations```.

In order to make a change to this directory, do the following:
 - copy the directory somewhere like in ```gpu_test_expectations_reverted```
 - in ```gpu_test_expectations_reverted``` run ```patch -p 1 -R angle-mods.patch```
 - do your changes in ```gpu_test_expectations```
 - delete angle-mods.path in both directories
 - run ```diff -rupN gpu_test_expectations_reverted gpu_test_expectations > angle-mods.patch```
 - copy ```angle-mods.patch``` in ```gpu_test_expectations```
