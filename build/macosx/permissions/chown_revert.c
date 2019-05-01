/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv) {
  if (argc != 2) return 1;

  uid_t realuser = getuid();
  char uidstring[20];
  snprintf(uidstring, 19, "%i", realuser);
  uidstring[19] = '\0';

  return execl("/usr/sbin/chown", "/usr/sbin/chown", "-R", "-h", uidstring,
               argv[1], (char*)0);
}
