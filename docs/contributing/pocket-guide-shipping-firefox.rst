Pocket Guide: Shipping Firefox
==============================

*Estimated read time:* 15min


Introduction
------------

The purpose of this document is to provide a high level understanding of
how Mozilla ships Firefox. With the intention of helping new Mozillians
(and those who would like a refresher) understand the basics of our
release process, tools, common terms, and mechanisms employed in
shipping Firefox to our users. Often this document will introduce a
concept, explain how it fits into the process, and then provide a link
to learn more if interested.

.. note::

  This does not contain an overview of how we
  ship :ref:`Fenix <fenix>` (Our Android browser) as
  that product is largely uncoupled from how we ship to desktop and the
  process we've historically followed.

Repositories & Channels
-----------------------

Shipping Firefox follows a software release :ref:`train model <train model>`
along 3 primary code :ref:`repositories <repositories>`; mozilla-central
(aka “m-c”), mozilla-beta, and mozilla-release. Each of these repositories are
updated within a defined cadence and built into one of our Firefox
products which are released through what is commonly referred to as
:ref:`Channels <channels>`: Firefox Nightly, Firefox Beta, and Firefox Release.

**Firefox Nightly** offers access to the latest cutting edge features
still under active development. Released every 12 hours with all the
changes that have :ref:`landed <landing>` on mozilla-central.

Every `4 weeks <https://wiki.mozilla.org/RapidRelease/Calendar>`__, we
:ref:`merge <merge>` the code from mozilla-central to our
mozilla-beta branch. New code or features can be added to mozilla-beta
outside of this 4 week cadence but will be required to land in
mozilla-central and then be :ref:`uplifted <uplift>` into
mozilla-beta.

**Firefox Beta** is for developers and early adopters who want to see
and test what’s coming next in Firefox. We release a new Beta version
three times a week.

.. note::

  The first and second beta builds of a new cycle are shipped to a
  subset of our Beta population. The full Beta population gets updated
  starting with Beta 3 only.*

Each Beta cycle lasts a total of 4 weeks where a final build is
validated by our QA and tagged for release into the mozilla-release
branch.

.. note::

  **Firefox Developer Edition** *is a separate product based on
  the mozilla-beta repo and is specifically tailored for Web Developers.*

**Firefox Release** is released every 4 weeks and is the end result
of our Beta cycle. This is our primary product shipping to hundreds of
millions of users. While a release is live, interim updates (dot releases)
are used to ship important bug fixes to users prior to the next major release.
These can happen on an as-needed basis when there is an important-enough
:ref:`driver <dot release drivers>` to do so (such as a critical bug severely
impairing the usability of the product for some users). In order to provide
better predictability, there is also a planned dot release scheduled for two
weeks after the initial go-live for less-critical fixes and other
:ref:`ride-along fixes <ride alongs>` deemed low-risk enough to include.

.. note::
  **Firefox ESR (Extended Support Release)** *is a separate
  product intended for Enterprise use. Major updates are rolled out once
  per year to maintain stability and predictability. ESR also
  contains a number of policy options not available in the standard
  Firefox Release. Minor updates are shipped in sync with the Firefox
  Release schedule for security and select quality fixes only.*

Further Reading/Useful links:

-  `Firefox Release
   Process <https://wiki.mozilla.org/Release_Management/Release_Process>`__
-  `Release
   Calendar <https://wiki.mozilla.org/Release_Management/Calendar>`__
-  `Firefox Delivery
   dashboard <https://mozilla.github.io/delivery-dashboard/>`__

Landing Code and Shipping Features
----------------------------------

Mozillians (those employed by MoCo and the broader community) land lots
of code in the Mozilla repositories: fixes, enhancements, compatibility,
new features, etc. and is managed by :ref:`Mercurial <Mercurial Overview>` (aka
hg). All new code is tracked in :ref:`Bugzilla <bugzilla>`, reviewed
in :ref:`Phabricator <Phabricator>`, and then checked into the
mozilla-central repository using :ref:`Lando <Lando>`.

.. note::

  Some teams use :ref:`GitHub <github>` during development
  but will still be required to use Phabricator (tracked in Bugzilla) to
  check their code into the mozilla-central hg repository.

The standard process for code to be delivered to our users is by ‘riding
the trains’, meaning that it’s landed in mozilla-central where it waits
for the next Beta cycle to begin. After merging to Beta the code will
stabilize over a 4 week period (along with everything else that merged
from mozilla-central). At the end of the beta cycle a release candidate
(:ref:`RC <rc>`) build will be generated, tested thoroughly, and
eventually become the next version of Firefox.

Further Reading/Useful links:

-  `Phabricator and why we use it <https://wiki.mozilla.org/Phabricator>`__
-  `Firefox Trello <https://trello.com/b/8k1hT2vh/firefox>`__ (Distilled
   list of critical features riding the trains)

An exception to this process...
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Not all code can simply wait for the normal train model to be included
in a Firefox build. There are a variety of reasons for this; critical
fixes, security concerns, stabilizing a feature that’s already in Beta,
shipping high priority features faster, and so on.

In these situations an uplift can be requested to take a recent landing
in mozilla-central and merge specific bits to another repository outside
the standard train model. After the request is made within Bugzilla,
:ref:`Release Management <release management>` will assess the potential risk
and will make a decision on whether it’s accepted.

Further Reading/Useful links:

-  `Patch uplifting
   rules <https://wiki.mozilla.org/Release_Management/Uplift_rules>`__

Ensuring build stability
~~~~~~~~~~~~~~~~~~~~~~~~

Throughout the process of landing code in mozilla-central to riding the
trains to Firefox Release, there are many milestones and quality
checkpoints from a variety of teams. This process is designed to ensure
a quality and compelling product will be consistently delivered to our
users with each new version. See below for a distilled list of those
milestones.

=========================================== ================ ================= ===============================================================================
Milestone                                   Week             Day of Week
------------------------------------------- ---------------- ----------------- -------------------------------------------------------------------------------
Merge Day                                   Nightly W1       Monday            Day 1 of the new Nightly Cycle
PI Request deadline                         Nightly W1       Friday            Manual QA request deadline for high risk features
Feature technical documentation due         Nightly W2       Friday            Deadline for features requiring manual QA
Beta release notes draft                    Nightly W4       Wednesday
Nightly features Go/No-Go decisions         Nightly W4       Wednesday
Feature Complete Milestone                  Nightly W4       Wednesday         Last day to land risky patches and/or enable new features
Nightly soft code freeze start              Nightly W4       Thursday          Stabilization period in preparation to merge to Beta
String freeze                               Nightly W4       Thursday          Modification or deletion of strings exposed to the end-users is not allowed
QA pre-merge regression testing completed   Nightly W4       Friday
Merge Day                                   Beta W1          Monday            Day 1 of the new Beta cycle
Pre-release sign off                        Beta W3          Friday            Final round of QA testing prior to Release
Firefox RC week                             Beta W4          Monday            Validating Release Candidate builds in preparation for the next Firefox Release
Release Notes ready                         Beta W4          Tuesday
What’s new page ready                       Beta W4          Wednesday
Firefox go-live @ 6am PT                    Release W1       Tuesday           Day 1 of the new Firefox Release to 25% of Release users
Firefox Release bump to 100%                Release W1       Thursday          Increase deployment of new Firefox Release to 100% of Release users
Scheduled dot release approval requests due Release W2       Friday            All requests required by EOD
Scheduled dot release go-live               Release W3       Tuesday           By default, ships when ready. Specific time available upon request.
=========================================== ================ ================= ===============================================================================


The Release Management team (aka “Relman”) monitors and enforces this
process to protect the stability of Firefox. Each member of Relman
rotates through end-to-end ownership of a given :ref:`release
cycle <release cycle>`. The Relman owner of a cycle will focus on the
overall release, blocker bugs, risks, backout rates, stability/crash
reports, etc. Go here for a complete overview of the `Relman Release
Process
Checklist <https://wiki.mozilla.org/Release_Management/Release_Process_Checklist_Documentation>`__.

.. note::

  While Relman will continually monitor the overall health of each
  Release it is the responsibility of the engineering organization to
  ensure the code they are landing is of high quality and the potential
  risks are understood. Every Release has an assigned :ref:`Regression
  Engineering Owner <reo>` (REO) to ensure a decision is made
  about each regression reported in the release.*

Further Reading/Useful links:

-  `Release Tracking
   Rules <https://wiki.mozilla.org/Release_Management/Tracking_rules>`__
-  `Release
   Owners <https://wiki.mozilla.org/Release_Management/Release_owners>`__
-  `Regression Engineering
   Owners <https://wiki.mozilla.org/Platform#Regression_Engineering_Owner_.28REO.29>`__
-  `Commonly used Bugzilla queries for all
   Channels <https://pascalc.net/rm_queries/>`__

Enabling/Disabling code (Prefs)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Within Firefox we allow the ability to Enable/Disable bits of code or
entire features using `Preferences <preferences>`. There are many
reasons why this is useful. Here are some examples:

-  Continual development over multiple release cycles without exposing
   partially completed features to our users
-  Provide the ability to quickly disable a feature if there is a
   problem found during the release process
-  Control features which are experimental or not ready to be shown to a
   specific channel population (e.g. enabled for Beta but disabled for
   Release)
-  A/B testing via :ref:`telemetry <telemetry>` experiments

.. note::

  :ref:`Normandy <normandy>` Pref Rollout is a feature that
  allows Mozilla to change the state of a preference for a targeted set of
  users, without deploying an update to Firefox. This is especially useful
  when conducting experiments or a gradual rollout of high risk features
  to our Release population.

Further Reading/Useful links:

-  `Brief guide to Mozilla
   preferences <https://developer.mozilla.org/en-US/docs/Mozilla/Preferences/A_brief_guide_to_Mozilla_preferences>`__
-  `Normandy Pref
   rollout <https://wiki.mozilla.org/Firefox/Normandy/PreferenceRollout>`__

Release & Feature QA
~~~~~~~~~~~~~~~~~~~~

Release QA is performed regularly and throughout the Release Cycle.
Organized in two-week sprints its primary goals are:

-  Qualifying builds for release
-  Feature testing
-  Product Integrity requests
-  Bug work
-  Community engagement

Features that can have significant impact and/or pose risk to the code
base should be nominated for QA support by the :ref:`feature
owner <feature owner>` in its intended release. This process is kicked
off by filing a :ref:`Product Integrity <product integrity>` team request
:ref:`PI request <pi request>`. These are due by the end of week 2
of the Nightly cycle.

.. note::

  Manual QA testing is only required for features as they go
  through the Beta cycle. Nightly Feature testing is always optional.

Further Reading/Useful links:

-  `QA Feature
   Testing <https://wiki.mozilla.org/QA/Feature_Testing_v2>`__
-  `Release QA
   overview <https://docs.google.com/document/d/1ic_3TO9-kNmZr11h1ZpyQbSlgiXzVewr3kSAP5ML4mQ/edit#heading=h.pvvuwlkkvtc4>`__
-  `PI Request template and
   overview <https://mana.mozilla.org/wiki/pages/viewpage.action?spaceKey=PI&title=PI+Request>`__

Experiments
~~~~~~~~~~~

As we deliver new features to our users we continually ask ourselves
about the potential impacts, both positive and negative. In many new
features we will run an experiment to gather data around these impacts.
A simple definition of an experiment is a way to measure how a change to
our product affects how people use it.

An experiment has three parts:

1. A new feature that can be selectively enabled
2. A group of users to test the new feature
3. Telemetry to measure how people interact with the new feature

Experiments are managed by an in-house tool called
`Experimenter <https://experimenter.services.mozilla.com/>`__.

Further Reading/Useful links:

-  `More about experiments and
   Experimenter <https://github.com/mozilla/experimenter>`__
-  `Requesting a new
   Experiment <https://experimenter.services.mozilla.com/experiments/new/>`__
   (Follow the ‘help’ links to learn more)
-  `Telemetry <https://wiki.mozilla.org/Telemetry>`__

Definitions
-----------

.. _bugzilla:

**Bugzilla** - Web-based general purpose bug tracking system and testing
tool

.. _channel:

**Channel** - Development channels producing concurrent releases of
Firefox for Windows, Mac, Linux, and Android

.. _dot release drivers:

**Dot Release Drivers** - Issues/Fixes that are significant enough to
warrant a minor dot release to the Firefox Release Channel. Usually to
fix a stability (top-crash) or Security (Chemspill) issue.

.. _feature owner:

**Feature Owner** - The person who is ultimately responsible for
developing a high quality feature. This is typically an Engineering
Manager or Product Manager.

.. _fenix:

**Fenix** - Also known as Firefox Preview is an all-new browser for
Android based on GeckoView and Android Components

.. _github:

**Github** - Web-based version control and collaboration platform for
software developers

.. _landing:

**Landing** - A general term used for when code is merged into a
particular source code repository

.. _lando:

**Lando** - Automated code lander for Mozilla. It is integrated with
our `Phabricator instance <https://phabricator.services.mozilla.com>`__
and can be used to land revisions to various repositories.

.. _mercurial:

**Mercurial** - A source-code management tool (just like git)
which allows users to keep track of changes to the source code
locally and share their changes with others. It is also called hg.

.. _merge:

**Merge** - General term used to describe the process of integrating and
reconciling file changes within the mozilla repositories

.. _normandy:

**Normandy** - Normandy is a collection of servers, workflows, and
Firefox components that enables Mozilla to remotely control Firefox
clients in the wild based on precise criteria

.. _orange_factor:

**Orange** - Also called flaky or intermittent tests. Describes a state
when a test or a testsuite can intermittently fail.

.. _phabricator:

**Phabricator** - Mozilla’s instance of the web-based software
development collaboration tool suite. Read more about `Phabricator as a
product <https://phacility.com/phabricator/>`__.

.. _pi request:

**PI Request** - Short for Product Integrity Request is a form
submission request that’s used to engage the PI team for a variety of
services. Most commonly used to request Feature QA it can also be used
for Security, Fuzzing, Performance, and many other services.

.. _preferences:

**Preferences** - A preference is any value or defined behavior that can
be set (e.g. enabled or disabled). Preference changes via user interface
usually take effect immediately. The values are saved to the user’s
Firefox profile on disk (in prefs.js).

.. _rc:

**Release Candidate** - Beta version with potential to be a final
product, which is ready to release unless significant bugs emerge.

.. _release cycle:

**Release Cycle** - The sum of stages of development and maturity for
the Firefox Release Product.

.. _reo:

**Regression Engineering Owner** - A partner for release management
assigned to each release. They both keep a mental state of how we are
doing and ensure a decision is made about each regression reported in
the release

.. _release engineering:

**Release engineering** - Team primarily responsible for maintaining
the build pipeline, the signature mechanisms, the update servers, etc.

.. _release management:

**Release Management** - Team primarily responsible for the process of
managing, planning, scheduling and controlling a software build through
different stages and environments

.. _Repository:

**Repository** - a collection of stored data from existing databases
merged into one so that it may be shared, analyzed or updated throughout
an organization

.. _ride alongs:

**Ride Alongs** - Bug fixes that are impacting release users but not
considered severe enough to ship without an identified dot release
driver.

.. _taskcluster:

**taskcluster** - Our execution framework to build, run tests on multiple
operating system, hardware and cloud providers.

.. _telemetry:

**Telemetry** - Firefox measures and collects non-personal information,
such as performance, hardware, usage and customizations. This
information is used by Mozilla to improve Firefox.

.. _train model:

**Train model** - a form of software release schedule in which a number
of distinct series of versioned software releases are released as a
number of different "trains" on a regular schedule.

.. _uplift:

**Uplift** - the action of taking parts from a newer version of a
software system (mozilla-central or mozilla-beta) and porting them to an
older version of the same software (mozilla-beta, mozilla-release or ESR)
