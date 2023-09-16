/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __RELRHACK_H__
#define __RELRHACK_H__

#include <elf.h>

#define DT_RELRHACK_BIT 0x8000000

#ifndef DT_RELRSZ
#  define DT_RELRSZ 35
#endif
#ifndef DT_RELR
#  define DT_RELR 36
#endif
#ifndef DT_RELRENT
#  define DT_RELRENT 37
#endif
#ifndef SHR_RELR
#  define SHT_RELR 19
#endif

#endif /* __RELRHACK_H__ */
