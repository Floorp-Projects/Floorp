# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import hashlib
import json
import logging

import requests


def fetch_data_for_cdms(cdms, urlParams):
    for cdm in cdms:
        if "fileName" in cdm:
            cdm["fileUrl"] = cdm["fileName"].format_map(urlParams)
            response = requests.get(cdm["fileUrl"])
            response.raise_for_status()
            cdm["hashValue"] = hashlib.sha512(response.content).hexdigest()
            cdm["filesize"] = len(response.content)


def generate_json_for_cdms(cdms):
    cdm_json = ""
    for cdm in cdms:
        if "alias" in cdm:
            cdm_json += (
                '        "{target}": {{\n'
                + '          "alias": "{alias}"\n'
                + "        }},\n"
            ).format_map(cdm)
        else:
            cdm_json += (
                '        "{target}": {{\n'
                + '          "fileUrl": "{fileUrl}",\n'
                + '          "filesize": {filesize},\n'
                + '          "hashValue": "{hashValue}"\n'
                + "        }},\n"
            ).format_map(cdm)
    return cdm_json[:-2] + "\n"


def calculate_gmpopenh264_json(version: str, version_hash: str, url_base: str) -> str:
    # fmt: off
    cdms = [
        {"target": "Darwin_aarch64-gcc3", "fileName": "{url_base}/openh264-macosx64-aarch64-{version}.zip"},
        {"target": "Darwin_x86_64-gcc3", "fileName": "{url_base}/openh264-macosx64-{version}.zip"},
        {"target": "Linux_x86-gcc3", "fileName": "{url_base}/openh264-linux32-{version}.zip"},
        {"target": "Linux_x86_64-gcc3", "fileName": "{url_base}/openh264-linux64-{version}.zip"},
        {"target": "Linux_x86_64-gcc3-asan", "alias": "Linux_x86_64-gcc3"},
        {"target": "Linux_aarch64-gcc3", "fileName": "{url_base}/openh264-linux64-aarch64-{version}.zip"},
        {"target": "WINNT_aarch64-msvc-aarch64", "fileName": "{url_base}/openh264-win64-aarch64-{version}.zip"},
        {"target": "WINNT_x86-msvc", "fileName": "{url_base}/openh264-win32-{version}.zip"},
        {"target": "WINNT_x86-msvc-x64", "alias": "WINNT_x86-msvc"},
        {"target": "WINNT_x86-msvc-x86", "alias": "WINNT_x86-msvc"},
        {"target": "WINNT_x86_64-msvc", "fileName": "{url_base}/openh264-win64-{version}.zip"},
        {"target": "WINNT_x86_64-msvc-x64", "alias": "WINNT_x86_64-msvc"},
        {"target": "WINNT_x86_64-msvc-x64-asan", "alias": "WINNT_x86_64-msvc"},
    ]
    # fmt: on
    try:
        fetch_data_for_cdms(cdms, {"url_base": url_base, "version": version_hash})
    except Exception as e:
        logging.error("calculate_gmpopenh264_json: could not create JSON due to: %s", e)
        return ""
    else:
        return (
            "{\n"
            + '  "hashFunction": "sha512",\n'
            + '  "name": "OpenH264-{}",\n'.format(version)
            + '  "schema_version": 1000,\n'
            + '  "vendors": {\n'
            + '    "gmp-gmpopenh264": {\n'
            + '      "platforms": {\n'
            + generate_json_for_cdms(cdms)
            + "      },\n"
            + '      "version": "{}"\n'.format(version)
            + "    }\n"
            + "  }\n"
            + "}"
        )


def calculate_widevinecdm_json(version: str, url_base: str) -> str:
    # fmt: off
    cdms = [
        {"target": "Darwin_aarch64-gcc3", "fileName": "{url_base}/{version}-mac-arm64.zip"},
        {"target": "Darwin_x86_64-gcc3", "alias": "Darwin_x86_64-gcc3-u-i386-x86_64"},
        {"target": "Darwin_x86_64-gcc3-u-i386-x86_64", "fileName": "{url_base}/{version}-mac-x64.zip"},
        {"target": "Linux_x86_64-gcc3", "fileName": "{url_base}/{version}-linux-x64.zip"},
        {"target": "Linux_x86_64-gcc3-asan", "alias": "Linux_x86_64-gcc3"},
        {"target": "WINNT_aarch64-msvc-aarch64", "fileName": "{url_base}/{version}-win-arm64.zip"},
        {"target": "WINNT_x86-msvc", "fileName": "{url_base}/{version}-win-x86.zip"},
        {"target": "WINNT_x86-msvc-x64", "alias": "WINNT_x86-msvc"},
        {"target": "WINNT_x86-msvc-x86", "alias": "WINNT_x86-msvc"},
        {"target": "WINNT_x86_64-msvc", "fileName": "{url_base}/{version}-win-x64.zip"},
        {"target": "WINNT_x86_64-msvc-x64", "alias": "WINNT_x86_64-msvc"},
        {"target": "WINNT_x86_64-msvc-x64-asan", "alias": "WINNT_x86_64-msvc"},
    ]
    # fmt: on
    try:
        fetch_data_for_cdms(cdms, {"url_base": url_base, "version": version})
    except Exception as e:
        logging.error("calculate_widevinecdm_json: could not create JSON due to: %s", e)
        return ""
    else:
        return (
            "{\n"
            + '  "hashFunction": "sha512",\n'
            + '  "name": "Widevine-{}",\n'.format(version)
            + '  "schema_version": 1000,\n'
            + '  "vendors": {\n'
            + '    "gmp-widevinecdm": {\n'
            + '      "platforms": {\n'
            + generate_json_for_cdms(cdms)
            + "      },\n"
            + '      "version": "{}"\n'.format(version)
            + "    }\n"
            + "  }\n"
            + "}"
        )


def main():
    examples = """examples:
  python dom/media/tools/generateGmpJson.py widevine 4.10.2557.0 >toolkit/content/gmp-sources/widevinecdm.json
  python dom/media/tools/generateGmpJson.py --url http://localhost:8080 openh264 2.3.1 0a48f4d2e9be2abb4fb01b4c3be83cf44ce91a6e"""

    parser = argparse.ArgumentParser(
        description="Generate JSON for GMP plugin updates",
        epilog=examples,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("plugin", help="which plugin: openh264, widevine")
    parser.add_argument("version", help="version of plugin")
    parser.add_argument("revision", help="revision hash of plugin", nargs="?")
    parser.add_argument("--url", help="override base URL from which to fetch plugins")
    args = parser.parse_args()

    if args.plugin == "openh264":
        url_base = "http://ciscobinary.openh264.org"
        if args.revision is None:
            parser.error("openh264 requires revision")
    elif args.plugin == "widevine":
        url_base = "https://redirector.gvt1.com/edgedl/widevine-cdm"
        if args.revision is not None:
            parser.error("widevine cannot use revision")
    else:
        parser.error("plugin not recognized")

    if args.url is not None:
        url_base = args.url

    if url_base[-1] == "/":
        url_base = url_base[:-1]

    if args.plugin == "openh264":
        json_result = calculate_gmpopenh264_json(args.version, args.revision, url_base)
    elif args.plugin == "widevine":
        json_result = calculate_widevinecdm_json(args.version, url_base)

    try:
        json.loads(json_result)
    except json.JSONDecodeError as e:
        logging.error("invalid JSON produced: %s", e)
    else:
        print(json_result)


main()
