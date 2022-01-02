/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unistd.h>

int main(int argc, char** argv) {
  if (argc != 2) return 1;

  return execl("/usr/sbin/chown", "/usr/sbin/chown", "-R", "-h", "root:admin",
               argv[1], (char*)0);
}
