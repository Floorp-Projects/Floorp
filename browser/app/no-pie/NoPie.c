/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
  // Ideally, we'd use mozilla::BinaryPath, but that pulls in stdc++compat,
  // and further causes trouble linking with LTO.
  char path[PATH_MAX + 4];
  ssize_t len = readlink("/proc/self/exe", path, PATH_MAX - 1);
  if (len < 0) {
    fprintf(stderr, "Couldn't find the application directory.\n");
    return 255;
  }
  strcpy(path + len, "-bin");
  execv(path, argv);
  // execv never returns. If it did, there was an error.
  fprintf(stderr, "Exec failed with error: %s\n", strerror(errno));
  return 255;
}
