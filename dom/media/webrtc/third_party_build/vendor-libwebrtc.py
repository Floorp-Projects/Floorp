# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import datetime
import os
import requests
import shutil
import subprocess
import sys
import tarfile


LIBWEBRTC_USED_IN_FIREFOX = [
    "AUTHORS",
    "LICENSE",
    "OWNERS",
    "PATENTS",
    "api",
    "audio",
    "call",
    "common_audio",
    "common_types.h",
    "common_video",
    "logging",
    "media",
    "modules",
    "rtc_base",
    "sdk/android",
    "system_wrappers",
    "video",
]


LIBWEBRTC_DIR = os.path.normpath("../../../../third_party/libwebrtc/webrtc")


def make_github_url(repo, commit):
    if not repo.endswith("/"):
        repo += "/"
    return repo + "archive/" + commit + ".tar.gz"


def make_googlesource_url(target, commit):
    if target == "libwebrtc":
        return "https://webrtc.googlesource.com/src.git/+archive/" + commit + ".tar.gz"
    else:
        return (
            "https://chromium.googlesource.com/chromium/src/build/+archive/"
            + commit
            + ".tar.gz"
        )


def fetch(target, url):
    print("Fetching commit from {}".format(url))
    req = requests.get(url)
    if req.status_code == 200:
        with open(target + ".tar.gz", "wb") as f:
            f.write(req.content)
    else:
        print(
            "Hit status code {} fetching commit. Aborting.".format(req.status_code),
            file=sys.stderr,
        )
        sys.exit(1)
    with open(os.path.join(LIBWEBRTC_DIR, "README.mozilla"), "w") as f:
        f.write(
            "libwebrtc updated from commit {} on {}.".format(
                url, datetime.datetime.utcnow().isoformat()
            )
        )


def fetch_local(target, path, commit):
    target_archive = target + ".tar.gz"
    cp = subprocess.run(["git", "archive", "-o", target_archive, commit], cwd=path)
    if cp.returncode != 0:
        print(
            "Hit return code {} fetching commit. Aborting.".format(cp.returncode),
            file=sys.stderr,
        )
        sys.exit(1)
    with open(os.path.join(LIBWEBRTC_DIR, "README.mozilla"), "w") as f:
        f.write(
            "libwebrtc updated from {} commit {} on {}.".format(
                path, commit, datetime.datetime.utcnow().isoformat()
            )
        )
    shutil.move(os.path.join(path, target_archive), target_archive)


def unpack(target):
    target_archive = target + ".tar.gz"
    target_path = "tmp-" + target
    try:
        shutil.rmtree(target_path)
    except FileNotFoundError:
        pass
    tarfile.open(target_archive).extractall(path=target_path)

    if target == "libwebrtc":
        for path in LIBWEBRTC_USED_IN_FIREFOX:
            try:
                shutil.rmtree(os.path.join(LIBWEBRTC_DIR, path))
            except FileNotFoundError:
                pass
            except NotADirectoryError:
                pass

        if os.path.exists(os.path.join(target_path, LIBWEBRTC_USED_IN_FIREFOX[0])):
            for path in LIBWEBRTC_USED_IN_FIREFOX:
                shutil.move(
                    os.path.join(target_path, path), os.path.join(LIBWEBRTC_DIR, path)
                )
        else:
            # GitHub packs everything inside a separate directory
            target_path = os.path.join(target_path, os.listdir(target_path)[0])
            for path in LIBWEBRTC_USED_IN_FIREFOX:
                shutil.move(
                    os.path.join(target_path, path), os.path.join(LIBWEBRTC_DIR, path)
                )
    else:
        try:
            shutil.rmtree(os.path.join(LIBWEBRTC_DIR, "build"))
        except FileNotFoundError:
            pass
        os.makedirs(os.path.join(LIBWEBRTC_DIR, "build"))

        if os.path.exists(os.path.join(target_path, "linux")):
            for path in os.listdir(target_path):
                shutil.move(
                    os.path.join(target_path, path),
                    os.path.join(LIBWEBRTC_DIR, "build", path),
                )
        else:
            # GitHub packs everything inside a separate directory
            target_path = os.path.join(target_path, os.listdir(target_path)[0])
            for path in os.listdir(target_path):
                shutil.move(
                    os.path.join(target_path, path),
                    os.path.join(LIBWEBRTC_DIR, "build", path),
                )


def cleanup(target):
    os.remove(target + ".tar.gz")
    shutil.rmtree("tmp-" + target)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Update libwebrtc")
    parser.add_argument("target", choices=("libwebrtc", "build"))
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--from-github", type=str)
    group.add_argument("--from-googlesource", action="store_true", default=False)
    group.add_argument("--from-local", type=str)
    parser.add_argument("--commit", type=str, default="master")
    parser.add_argument("--skip-fetch", action="store_true", default=False)
    parser.add_argument("--skip-cleanup", action="store_true", default=False)
    args = parser.parse_args()

    os.makedirs(LIBWEBRTC_DIR, exist_ok=True)

    if not args.skip_fetch:
        if args.from_github:
            fetch(args.target, make_github_url(args.from_github, args.commit))
        elif args.from_googlesource:
            fetch(args.target, make_googlesource_url(args.target, args.commit))
        elif args.from_local:
            fetch_local(args.target, args.from_local, args.commit)
    unpack(args.target)
    if not args.skip_cleanup:
        cleanup(args.target)
