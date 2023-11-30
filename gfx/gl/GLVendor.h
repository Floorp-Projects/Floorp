/* -*- mode: c++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this
 * file, you can obtain one at http://mozilla.org/mpl/2.0/. */

#ifndef GLVENDOR_H_
#define GLVENDOR_H_

#include "mozilla/DefineEnum.h"

namespace mozilla {
namespace gl {
MOZ_DEFINE_ENUM(GLVendor, (Intel, NVIDIA, ATI, Qualcomm, Imagination, Nouveau,
                           Vivante, VMware, ARM, Other));

}
}  // namespace mozilla

#endif /* GLVENDOR_H_ */
