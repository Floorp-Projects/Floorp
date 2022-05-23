## Welcome to the official Floorp browser repository.

[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]

[1]: https://en.wikipedia.org/wiki/Hobbit#Lifestyle

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/Floorp-Projects/Floorp.svg?style=for-the-badge
[contributors-url]: https://github.com/Floorp-Projects/Floorp/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/Floorp-Projects/Floorp?style=for-the-badge
[forks-url]: https://github.com/Floorp-Projects/Floorp/network/members
[stars-shield]: https://img.shields.io/github/stars/Floorp-Projects/Floorp.svg?style=for-the-badge
[stars-url]: https://github.com/Floorp/Floorp-Projects/stargazers
[issues-shield]: https://img.shields.io/github/issues/Floorp-Projects/Floorp.svg?style=for-the-badge
[issues-url]: https://github.com/Floorp-Projects/Floorp-Projects/issues
[license-shield]: https://img.shields.io/github/license/Floorp-Projects/Floorp.svg?style=for-the-badge
[license-url]: https://github.com/Floorp-Projects/Floorp/blob/master/LICENSE
<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/Floorp-Projects/Floorp">
    <img src="https://github.com/Floorp-Projects/Floorp/blob/master/browser/branding/official/default256.png" alt="Logo" width="150" height="150">
  </a>

  <h3 align="center">Floorp Browser</h3>

  <p align="center">
    Mozilla Firefox derivative, browser development project
    <br />
    <a href="https://github.com/Floorp-Projects/about-floorp-projects">Explore the docs »</a>
    <br />
    <br />
    <a href="https://floorp.ablaze.one">official site</a>
    ・
    <a href="https://github.com/Floorp-Projects/Floorp-dev">Floorp Browser source code</a>
    ・
    <a href="https://blog.ablaze.one">Blog & release notes</a>
    ・
    <a href="https://support.ablaze.one">official support site & Send feedback</a>
  </p>
</div>

---

### Repository Overview

This repository contains source code for the Floorp legacy browser (Floorp browser) from version 10.0.0 and later.

Branches ```master```, ```release```, and ``beta`` exist, and may contain branches created temporarily to create Linux versions.

The ``master`` is the Nighly version of Firefox, and is used by the Floorp developers to envision future features. Therefore, not all source code falls into the ``release``.

```beta``` is the version of Floorp that will be released next; Firefox is a Beta/Developer version, which contains code that will actually be released, but not all features may be applicable to ``release``.

```release``` is the source code for the current release of the browser, which is the same version as the stable version of Firefox.

----

### About issues and pull requests

Feel free to use ``issues`` as long as they are Floorp related. If you are familiar with user.js or have some knowledge of the browser, `issues` are easier to understand and will save time for developers and make development and operation smoother.

A `pull request` can be made if the user is able to fix the bug discussed in the `issue`. However, you may also submit translations or fixes to code you are interested in without regard to the `issue`. However, please provide the code in as clear a form as possible. Also, we would appreciate it if you could submit your code to the `beta` branch instead of the `release` branch if at all possible. However, we cannot guarantee that 100% of the code you submit will be used.

If it is used, we will include your name in the release notes with a link to your GitHub profile and site/Twitter account.

---

## Support and release system

### Operating Hours

Floorp is currently being developed completely on a non-profit basis, so the management does not have that many resources available. In other words, the response time is vague. We will respond when we can.

### Available official support

Floorp is available on Twitter, our support site, and GitHub. Support Site
We are also posting questions that we have received in the past on our support site.

### Floorp's Release Structure

Floorp is a rapid release browser, like Firefox, in order to maximize security and privacy protection. However, the release phase is roughly the same as Firefox.

Specifically, the release phases are as follows

Create a patch for the next version of Firefox on the beta branch before the next version of Firefox is released. The beta version is released with the patch applied. Do this for two and a half weeks to create Floorp features and keep up with the latest version of Firefox.

The source code for the release version of the next version of Firefox will be made available one week before. This source code is relatively stable because it has gone through a beta version. However, it is not complete source code. However, waiting for this will delay security fixes, so Floorp developers will start building Floorp four days after the source code is released. This strikes a balance between keeping the latest version and fixing security issues.

The exception to this is that if additional critical vulnerabilities or other fixes are introduced into Firefox, Floorp developers may build with additional patches on a case-by-case basis to release new features earlier. In the case of minor updates, Floorp may ignore them. This is the Floorp release method.
