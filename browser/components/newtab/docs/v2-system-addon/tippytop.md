# TippyTop in Activity Stream
TippyTop, a collection of icons from the Alexa top sites, provides high quality images for the Top Sites in Activity Stream. The TippyTop manifest is hosted on S3, and then moved to [Remote Settings](https://remote-settings.readthedocs.io/en/latest/index.html) since Firefox 63. In this document, we'll cover how we produce and manage TippyTop manifest for Activity Stream.

## TippyTop manifest production
TippyTop manifest is produced by [tippy-top-sites](https://github.com/mozilla/tippy-top-sites).

```sh
# set up the environment, only needed for the first time
$ pip install -r requirements.txt
$ python make_manifest.py --count 2000 > icons.json  # Alexa top 2000 sites
```

Because the manifest is hosted remotely, we use another repo [tippytop-service](https://github.com/mozilla-services/tippytop-service) for the version control and deployment. Ask :nanj or :r1cky for permission to access this private repo.

## TippyTop manifest publishing
For each new manifest release, firstly you should tag it in the tippytop-service repo, then publish it as follows:

### For Firefox 62 and below
File a deploy bug with the tagged version at Bugzilla as [Activity Streams: Application Servers](https://bugzilla.mozilla.org/enter_bug.cgi?product=Firefox&component=Activity%20Streams%3A%20Application%20Servers), assign it to our system engineer :jbuck, he will take care of the rest.

### For Firefox 63 and beyond
Activity Stream started using Remote Settings to manage TippyTop manifest since Firefox 63. To be able to publish new manifest, you need to be in the author&reviewer group of Remote Settings. See more details in this [mana page](https://mana.mozilla.org/wiki/pages/viewpage.action?pageId=66655528). You can also ask :nanj or :leplatram to get this set up for you.
To publish the manifest to Remote Settings, go to the tippytop-service repo, and run the script as follows,

```sh
# set up the remote setting, only needed for the first time
$ python3 -m venv .venv
$ source .venv/bin/activate
$ pip install -r requirements.txt

# publish it to prod
$ source .venv/bin/activate
# It will ask you for your LDAP user name and password.
$ ./upload2remotesettings.py prod
```

After uploading it to Remote Setting, you can request for review in the [dashboard](https://settings-writer.prod.mozaws.net/v1/admin/). Note that you will need to log in the Mozilla LDAP VPN for both uploading and accessing Remote Setting's dashboard. Once your request gets approved by the reviewer, the new manifest will be content signed and published to production.

## TippyTop Viewer
You can use this [viewer](https://mozilla.github.io/tippy-top-sites/manifest-viewer/) to load all the icons in the current manifest.
