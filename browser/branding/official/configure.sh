# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

MOZ_APP_DISPLAYNAME=Firefox

if test "$MOZ_UPDATE_CHANNEL" = "beta"; then
  # Official beta builds
  MOZ_HANDLER_CLSID="21e9f98d-a6c9-4cb5-b288-ae2fd2a96c58"
  MOZ_IHANDLERCONTROL_IID="119149fa-d212-4f22-9517-082eecc1a084"
  MOZ_ASYNCIHANDLERCONTROL_IID="4e253d9b-59cf-4b32-a973-38bc85495d61"
  MOZ_IGECKOBACKCHANNEL_IID="77b75c7d-d1c2-4469-864d-31aaebb67cc6"
else
  # Official release/esr builds
  MOZ_HANDLER_CLSID="1baa303d-b4b9-45e5-9ccb-e3fca3e274b6"
  MOZ_IHANDLERCONTROL_IID="ce30f77e-8847-44f0-a648-a9656bd89c0d"
  MOZ_ASYNCIHANDLERCONTROL_IID="dca8d857-1a63-4045-8f36-8809eb093d04"
  MOZ_IGECKOBACKCHANNEL_IID="b32983ff-ef84-4945-8f86-fb7491b4f57b"
fi
