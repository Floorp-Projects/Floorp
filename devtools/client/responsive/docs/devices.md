# Updating List of Devices for RDM

## Where to locate the list

The devices list is a [RemoteSettings](https://firefox-source-docs.mozilla.org/services/settings/index.html) collection named `devtools-devices`.
A dump of the list can be found in [services/settings/dumps/main/devtools-devices.json](https://searchfox.org/mozilla-central/source/services/settings/dumps/main/devtools-devices.json).

## Adding and removing devices

There is no set criteria for which devices should be added or removed from the list. However, we can take this into account:

- Adding the latest iPhone and iPad models from Apple.
- Adding the latest Galaxy series from Samsung.
- Checking out what devices Google Chrome supports in its DevTools. They keep the list hardcoded in [source](https://github.com/ChromeDevTools/devtools-frontend/blob/79095c6c14d96582806982b63e99ef936a6cd74c/front_end/models/emulation/EmulatedDevices.ts#L645)

## Data format

An important field is `featured`, which is a boolean. When set to `true`, the device will appear in the RDM dropdown. If it's set to `false`, the device will not appear in the dropdown, but can be enabled in the `Edit list` modal.
Each device has a user agent specified. We can get this value by:

- At `https://developers.whatismybrowser.com/useragents/explore/`
- With a real device, open its default browser, and google "my user agent" will display a Google widget with the user agent string.
- Looking at Google's own list of devices (they also specify the user agent)

## Releasing the changes

First, make sure you can have access to RemoteSettings (see https://remote-settings.readthedocs.io/en/latest/getting-started.html#getting-started).

You should then be able to add the device to the [RemoteSettings Stage instance](https://settings-writer.stage.mozaws.net/v1/admin/#/buckets/main-workspace/collections/devtools-devices/) using the interface.
Then use the RemoteSettings DevTools to make Firefox pull the devices list from the Stage instance (see https://remote-settings.readthedocs.io/en/latest/support.html?highlight=devtools#how-do-i-setup-firefox-to-pull-data-from-stage)
Once that is done, open RDM and make sure you can see the new addition in the Devices modal.

If everything is good, you can then ask for review on the data change. Once this get approved, you can replicate the same changes to the [RemoteSettings Prod instance](https://settings-writer.prod.mozaws.net/v1/admin/#/buckets/main-workspace/collections/devtools-devices/), reset the RemoteSettings DevTools settings, check RDM again just to be sure and finally ask for review on the data change.

## Things to consider in the future

- Galaxy Fold has two screens, how do we handle that?
