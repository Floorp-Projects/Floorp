========================
Installation Attribution
========================

*Installation Attribution* is a system that allows us to do a few things:

- Gauge the success of marketing campaigns
- Determine (roughly) the origin of an installer that a user ran
- Support the Return to AMO workflow.

We accomplish these things by adding an *attribution code* to Firefox installers which generally contains information supplied by www.mozilla.org (Bedrock and related services). This information is read by Firefox during and after installation, and sent back to Mozilla through Firefox Telemetry.

The following information is supported by this system:

* Traditional `UTM parameters <https://en.wikipedia.org/wiki/UTM_parameters>`_ (*source*, *medium*, *campaign*, and *content*)
* *experiment*
* *variation*
* *ua*
* *dltoken*
* *dlsource*
* *msstoresignedin*

Descriptions of each of these can be found in :ref:`the Telemetry Environment documentation <environment>`.

---------------------------------------
Firefox Windows Installers & macOS DMGs
---------------------------------------

Installs done through Windows stub or full NSIS installers or macOS DMGs are capable of being attributed. When these packages are created, they are given initial attribution data of *dlsource=mozillaci*. Users who download their package via www.mozilla.org will typically have this attribution data overwritten (unless they have Do-not-track (DNT) enabled), with *dlsource=mozorg*, a *dltoken*, and whatever UTM parameters Bedrock deems appropriate.

An additional complication here is that the attribution system is used (or abused, depending on your view) to support the Return to AMO workflow -- forcing the *campaign* and *content* UTM parameters to specific values.

The below diagram illustrates the flow of the cases above:

.. mermaid::

    flowchart TD
        subgraph Legend
            direction LR
            start1[ ] --->|"(1) Bedrock, Do-Not-Track enabled"| stop1[ ]
            start2[ ] --->|"(2) Bedrock, Do-Not-Track disabled"| stop2[ ]
            start3[ ] --->|"(3) Bedrock via Search Engine"| stop3[ ]
            start4[ ] --->|"(4) Bedrock via Return to AMO flow"| stop4[ ]
            start5[ ] --->|"(5) Direct download from CDN Origin"| stop5[ ]
            start6[ ] --->|"Common Paths"| stop6[ ]

            %% Legend colours
            linkStyle 0 stroke-width:2px,fill:none,stroke:blue;
            linkStyle 1 stroke-width:2px,fill:none,stroke:green;
            linkStyle 2 stroke-width:2px,fill:none,stroke:red;
            linkStyle 3 stroke-width:2px,fill:none,stroke:purple;
            linkStyle 4 stroke-width:2px,fill:none,stroke:pink;
        end

        subgraph Download Flow
            User([User])
            Search[Search Engine]
            AMO[AMO<br>addons.mozilla.org]
            Bedrock[Bedrock<br>mozilla.org]
            Bouncer[Bouncer<br>download.mozilla.org]
            CDNOrigin[CDN Origin<br>archive.mozilla.org]
            CDN[CDN<br>download-installer.cdn.mozilla.net]
            Attr[Attribution Service<br>stubdownloader.services.mozilla.net]
            AttrCDN[Attribution CDN<br>cdn.stubdownloader.services.mozilla.net]
            Inst([Installer Downloaded])

            %% Case 1: Bedrock, DNT enabled
            User ----> Bedrock
            Bedrock ---->|"No attribution data set"| Bouncer
            Bouncer ----> CDN
            CDN ---->|"Contains static attribution data:<br><i>dlsource</i>: mozillaci"| Inst

            linkStyle 6,7,8,9 stroke-width:2px,fill:none,stroke:blue;

            %% Case 2: Bedrock, DNT disabled
            User ---> Bedrock

            linkStyle 10 stroke-width:2px,fill:none,stroke:green;

            %% Case 3: Bedrock via Search Engine
            User ----> Search
            Search ---->|"Sets UTM parameters"| Bedrock

            linkStyle 11,12 stroke-width:2px,fill:none,stroke:red;

            %% Case 4: Bedrock via Return to AMO flow
            User ---->|"Initiates Return-to-AMO request"| AMO
            AMO ---->|"Sets <i>campaign</i> and<br><i>content</i> UTM parameters"| Bedrock

            linkStyle 13,14 stroke-width:2px,fill:none,stroke:purple;

            %% Case 5: Direct download from CDN
            User --> CDNOrigin
            CDNOrigin -->|"Contains static attribution data:<br><i>dlsource</i>: mozillaci"| Inst

            linkStyle 15,16 stroke-width:2px,fill:none,stroke:pink;

            %% Common links for cases 2, 3, and 4
            Bedrock ---->|"Attribution data forwarded:<br><i>dlsource</i>: mozorg<br><i>dltoken</i>: present<br>any UTM parameters set"| Bouncer
            Bouncer ---->|"Forwards attribution data"| Attr
            Attr <---->|"Fetches installer"| CDN
            Attr ---->|"Places modified installer<br>on Attribution CDN"| AttrCDN
            AttrCDN ---->|"Contains dynamic attribution data:<br><i>dlsource</i>: mozorg<br><i>dltoken</i>: present<br>any UTM parameters set"| Inst

            %% Common links for everything
            CDN <---->|"Fetches installer"| CDNOrigin
        end

~~~~~~~
Windows
~~~~~~~

Windows attribution is implementing by injecting data into the signature block of NSIS installers at download time. This technique is described in the "Cheating Authenticode" section of `this Microsoft blog post <https://learn.microsoft.com/en-ca/archive/blogs/ieinternals/caveats-for-authenticode-code-signing#cheating-authenticode>`_.

~~~~~
macOS
~~~~~

macOS attribution is implemented by adding a ``com.apple.application-instance`` extended attribute to the ``.app`` bundle at download time. This special extended attribute is explicitly *not* part of the digital signature of the ``.app`` bundle as per `this Apple technical note <https://developer.apple.com/library/archive/technotes/tn2206/_index.html#//apple_ref/doc/uid/DTS40007919-CH1-TNTAG401>`_.


---------------
Microsoft Store
---------------

Firefox installs done through the Microsoft Store support extracting campaign IDs that may be embedded into them. This allows us to attribute installs through different channels by providing particular links to the Microsoft Store with attribution data included. For example:

`ms-windows-store://pdp/?productid=9NZVDKPMR9RD&cid=source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set) <ms-windows-store://pdp/?productid=9NZVDKPMR9RD&cid=source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set)>`_


`https://www.microsoft.com/store/apps/9NZVDKPMR9RD?cid=source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set) <https://www.microsoft.com/store/apps/9NZVDKPMR9RD?cid=source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set)>`_


For more on how custom campaign IDs work in general in the Microsoft Store environment, `see Microsoft's documentation <https://docs.microsoft.com/en-us/windows/uwp/publish/create-a-custom-app-promotion-campaign>`_.

The Microsoft Store provides a single `cid` (Campaign ID). Their documentation claims it is limited to 100 characters, although in our own testing we've been able to retrieve the first 208 characters of Campaign IDs. Firefox expects this Campaign ID to follow the same format as stub and full installer attribution codes, which have a maximum of length of 1010 characters. Since Campaign IDs are more limited than what Firefox allows, we need to be a bit more thoughtful about what we include in them vs. stub and full installer attribution. At the time of writing, we've yet to be able to test whether we can reliably pull more than the advertised 100 characters of a Campaign ID in the real world -- something that we should do before we send any crucial information past the first 100 characters.

In addition to the attribution data retrieved through the campaign ID, we also add an extra key to it to indicate whether or not the user was signed into the Microsoft Store when they installed. This `msstoresignedin` key can have a value of `true` or `false`.

There are a couple of other caveats to keep in mind:

* A campaign ID is only set the *first* time a user installs Firefox through the Store. Subsequent installs will inherit the original campaign ID (even if it was an empty string). This means that only brand new installs will be attributed -- not reinstalls.
* At the time of writing, it is not clear whether or not installs done without being signed into the Microsoft Store will be able to find their campaign ID. Microsoft's documentation claims they can, but our own testing has not been able to verify this.
