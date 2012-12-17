# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

DOM_SRCDIRS = \
  dom/base \
  dom/battery \
  dom/encoding \
  dom/file \
  dom/power \
  dom/quota \
  dom/media \
  dom/network/src \
  dom/settings \
  dom/phonenumberutils \
  dom/sms/src \
  dom/contacts \
  dom/permission \
  dom/alarm \
  dom/src/events \
  dom/src/storage \
  dom/src/offline \
  dom/src/geolocation \
  dom/src/notification \
  dom/workers \
  dom/time \
  content/xbl/src \
  content/xul/document/src \
  content/events/src \
  content/base/src \
  content/html/content/src \
  content/html/document/src \
  content/media/webaudio \
  content/svg/content/src \
  layout/generic \
  layout/style \
  layout/xul/base/src \
  layout/xul/base/src/tree/src \
  dom/camera \
  $(NULL)

ifdef MOZ_B2G_RIL
DOM_SRCDIRS += \
  dom/system/gonk \
  dom/telephony \
  dom/wifi \
  dom/icc/src \
  $(NULL)
endif

ifdef MOZ_B2G_FM
DOM_SRCDIRS += \
  dom/fm \
  $(NULL)
endif

ifdef MOZ_B2G_BT
DOM_SRCDIRS += dom/bluetooth
endif

LOCAL_INCLUDES += $(DOM_SRCDIRS:%=-I$(topsrcdir)/%)
DEFINES += -D_IMPL_NS_LAYOUT
