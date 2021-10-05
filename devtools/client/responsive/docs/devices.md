# Updating List of Devices for RDM

## Where to locate the list
The devices list is a JSON file that is being served in a CDN (currently an Amazon S3 bucket) at `https://code.cdn.mozilla.net/devices/devices.json`
We do not have this list in mozilla-central, but in a separate GitHub  repository `https://github.com/mozilla/simulated-devices`.

## Adding and removing devices
There is no set criteria for which devices should be added or removed from the list. However, we can take this into account:
- Adding the latest iPhone and iPad models from Apple.
- Adding the latest Galaxy series from Samsung.
- Checking out what devices Google Chrome supports in its DevTools. They keep the list hardcoded in [source](https://github.com/ChromeDevTools/devtools-frontend/blob/79095c6c14d96582806982b63e99ef936a6cd74c/front_end/models/emulation/EmulatedDevices.ts#L645)

## File format
It's JSON. Each device is represented by an Object.
An important field is `featured`, which is a boolean. When set to `true`, the device will appear in the RDM dropdown. If it's set to `false`, the device will not appear in the dropdown, but can be enabled in the `Edit list` modal.
Each device has a user agent specified. We can get this value by:
- At `https://developers.whatismybrowser.com/useragents/explore/`
- With a real device, open its default browser, and google  "my user agent" will display a Google widget with the user agent string.
- Looking at  Google's own list of devices (they also specify the user agent)

## Testing the list locally
- Start a local web server to serve `devices.json`
- Open firefox and set the pref devtools.devices.url to the devices.json URL (ex: `http://127.0.0.1:8080/devices.json`) The default value is: `https://code.cdn.mozilla.net/devices/devices.json`
- Open DevTools and RDM
Note that this list is cached. If the JSON file gets updated, it will need to be served with a different URL for the list to be updated in RDM.

## Releasing the changes
- Clone `https://github.com/mozilla/simulated-devices` repo
- Modify `devices.json`
- Use node `test.js` to verify your `devices.json` file.
- Submit [pull request](https://github.com/mozilla/simulated-devices/pulls)
- Request CDN access to the bucket in JIRA (or ask Jan Odvarko to update CDN)

## Things to consider in the future
- The GitHub repository barely has any activity, this should probably be source controlled by a Mozilla property.
- [Remote Settings](https://remote-settings.readthedocs.io/en/latest/) is probably a better option for this.
It has a developer API
- It already has a (documented) and established process to get access to it.
- Galaxy Fold has two screens, how do we handle that?
