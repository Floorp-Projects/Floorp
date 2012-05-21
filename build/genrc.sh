#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

DATATYPE="$1"
INFILE="$2"

echo "${DATATYPE} RCDATA"
sed 's/"/""/g' ${INFILE} | awk 'BEGIN { printf("BEGIN\n") } { printf("\"%s\\r\\n\",\n", $0) } END { printf("\"\\0\"\nEND\n") }'

exit 0
