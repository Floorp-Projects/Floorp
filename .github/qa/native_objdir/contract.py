import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import stat
import struct
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile

REPOSITORY = "Floorp-Projects/Floorp-Runtime"
SOURCE = "7dd619742b25567e479eeda97ae071d10542ad0b"
RUN = 38885821823
ARTIFACT = 12047907028
NAME = "floorp-mac-x86_64-build-with-profgen"
ZIP_BYTES = 5470281993
ZIP_SHA = "de764486eb0cbcbde256fcdce4b666408a6f4badfa1e9b5f21aad121976d911d"
ENTRY = "floorp-mac-x86_64-moz-artifact.tar.gz"
MAX_GZIP = 6 * 1024**3


def require(condition, code):
    if not condition:
        raise ValueError(code)


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def api(path, token):
    request = urllib.request.Request(
        "https://api.github.com/repos/" + REPOSITORY + path,
        headers={"Authorization": "Bearer " + token,
                 "Accept": "application/vnd.github+json",
                 "X-GitHub-Api-Version": "2022-11-28"})
    with urllib.request.build_opener(NoRedirect()).open(request, timeout=20) as response:
        require(response.status == 200, "api-status")
        data = response.read(524289)
    require(len(data) <= 524288, "api-size")
    return json.loads(data)


def download_location(token):
    request = urllib.request.Request(
        "https://api.github.com/repos/" + REPOSITORY +
        "/actions/artifacts/" + str(ARTIFACT) + "/zip",
        headers={"Authorization": "Bearer " + token,
                 "X-GitHub-Api-Version": "2022-11-28"})
    try:
        with urllib.request.build_opener(NoRedirect()).open(request, timeout=20):
            raise ValueError("expected-redirect")
    except urllib.error.HTTPError as error:
        if error.code != 302:
            raise
        location = error.headers.get("Location", "")
    parts = urllib.parse.urlsplit(location)
    require(parts.scheme == "https" and not parts.username and
            parts.hostname is not None and
            (parts.hostname.endswith(".blob.core.windows.net") or
             parts.hostname.endswith(".actions.githubusercontent.com")), "storage-origin")
    return location


class HeaderReader(io.RawIOBase):
    def __init__(self, location):
        self.location = location
        self.position = 0
        self.transferred = 0
        self.requests = 0
        self.started = time.monotonic()

    def seekable(self):
        return True

    def readable(self):
        return True

    def tell(self):
        return self.position

    def seek(self, offset, whence=0):
        require(whence in (0, 1, 2), "seek-origin")
        result = offset + (self.position if whence == 1 else ZIP_BYTES if whence == 2 else 0)
        require(0 <= result <= ZIP_BYTES, "seek-limit")
        self.position = result
        return result

    def read(self, size=-1):
        if size < 0:
            size = ZIP_BYTES - self.position
        size = min(size, ZIP_BYTES - self.position)
        if not size:
            return b""
        require(size <= 262144 and self.transferred + size <= 1048576 and
                self.requests < 16 and time.monotonic() - self.started < 90,
                "header-transfer-budget")
        start, end = self.position, self.position + size - 1
        request = urllib.request.Request(self.location,
            headers={"Range": f"bytes={start}-{end}", "Accept-Encoding": "identity"})
        self.requests += 1
        with urllib.request.build_opener(NoRedirect()).open(request, timeout=20) as response:
            require(response.status == 206 and response.headers.get("Content-Range") ==
                    f"bytes {start}-{end}/{ZIP_BYTES}", "range-contract")
            data = response.read(size + 1)
        require(len(data) == size, "range-size")
        self.position += size
        self.transferred += size
        return data


def zip64_extra(data):
    result = None
    while data:
        require(len(data) >= 4, "extra-header")
        tag, size = struct.unpack("<HH", data[:4])
        require(tag == 1 and result is None and size in (8, 16, 24, 28) and
                len(data) >= 4 + size, "extra-contract")
        result, data = data[4:4 + size], data[4 + size:]
    return result


def zip_header_contract(reader):
    with zipfile.ZipFile(reader) as bundle:
        entries = bundle.infolist()
        require(len(entries) == 1 and not bundle.comment, "single-entry")
        entry = entries[0]
        zip64_extra(entry.extra)
        mode = entry.external_attr >> 16
        require(entry.filename == ENTRY and not entry.is_dir() and
                stat.S_IFMT(mode) in (0, stat.S_IFREG) and not stat.S_ISLNK(mode) and
                not entry.flag_bits & 1 and entry.header_offset == 0 and
                entry.compress_type == zipfile.ZIP_DEFLATED and
                0 < entry.file_size <= MAX_GZIP, "entry-contract")
        reader.seek(0)
        header = reader.read(30)
        values = struct.unpack("<4s5H3I2H", header)
        require(values[0] == b"PK\x03\x04" and values[2] == entry.flag_bits and
                values[3] == entry.compress_type and values[-2] == len(ENTRY), "local-header")
        name_and_extra = reader.read(values[-2] + values[-1])
        require(name_and_extra[:values[-2]] == ENTRY.encode(), "local-name")
        local_zip64 = zip64_extra(name_and_extra[values[-2]:])
        data_end = 30 + values[-2] + values[-1] + entry.compress_size
        gap = bundle.start_dir - data_end
        if entry.flag_bits & 8:
            require(values[6] in (0, entry.CRC), "descriptor-local-crc")
            local_compressed, local_uncompressed = values[7:9]
            if local_zip64 is not None:
                require(len(local_zip64) == 16, "descriptor-local-zip64")
                local_uncompressed, local_compressed = struct.unpack("<QQ", local_zip64)
            require(local_compressed in (0, entry.compress_size) and
                    local_uncompressed in (0, entry.file_size), "descriptor-local-sizes")
            require(gap in (12, 16, 20, 24), "descriptor-layout")
            reader.seek(data_end)
            descriptor = reader.read(gap)
            if descriptor.startswith(b"PK\x07\x08"):
                descriptor = descriptor[4:]
            require(len(descriptor) in (12, 20), "descriptor-size")
            crc, compressed, uncompressed = struct.unpack(
                "<III" if len(descriptor) == 12 else "<IQQ", descriptor)
            require((crc, compressed, uncompressed) ==
                    (entry.CRC, entry.compress_size, entry.file_size), "descriptor-values")
        else:
            require(gap == 0 and values[6] == entry.CRC, "local-layout")
            compressed, uncompressed = values[7:9]
            value = local_zip64 or b""
            if uncompressed == 0xffffffff:
                require(len(value) >= 8, "zip64-uncompressed")
                uncompressed, = struct.unpack("<Q", value[:8])
                value = value[8:]
            if compressed == 0xffffffff:
                require(len(value) >= 8, "zip64-compressed")
                compressed, = struct.unpack("<Q", value[:8])
            require((compressed, uncompressed) == (entry.compress_size, entry.file_size),
                    "local-sizes")
        with bundle.open(entry):
            pass
        return {"entryName": ENTRY, "entryBytes": entry.file_size,
                "entryCRC32": entry.CRC, "compressionMethod": entry.compress_type,
                "singleRegularEntry": True, "localCentralNamesMatch": True,
                "physicalSingleEntryLayoutVerified": True,
                "headerBytesRead": reader.transferred, "headerRequests": reader.requests}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--payload", type=Path, required=True)
    args = parser.parse_args()
    report = {"status": "failed", "scope": "objdir-read-only-inspection",
              "repository": REPOSITORY, "sourceRun": RUN, "sourceAttempt": 1,
              "sourceCommit": SOURCE, "artifactId": ARTIFACT,
              "providerZIPBytes": ZIP_BYTES, "providerZIPDigest": "sha256:" + ZIP_SHA}
    stage = "scope"
    try:
        require(os.environ.get("GITHUB_REPOSITORY") == REPOSITORY and
                os.environ.get("RUNNER_OS") == "Linux" and
                os.environ.get("RUNNER_ARCH") == "X64", "runner-scope")
        require(not args.payload.exists(), "payload-already-exists")
        free = shutil.disk_usage(args.output.parent).free
        require(free >= MAX_GZIP + 2 * 1024**3, "disk-budget")
        stage = "api-metadata"
        token = os.environ["GITHUB_TOKEN"]
        run = api("/actions/runs/" + str(RUN), token)
        require(run["id"] == RUN and run["head_sha"] == SOURCE and
                run["run_attempt"] == 1 and run["status"] == "completed" and
                run["conclusion"] == "success" and run["repository"]["full_name"] == REPOSITORY,
                "source-run")
        artifact = api("/actions/artifacts/" + str(ARTIFACT), token)
        require(artifact["id"] == ARTIFACT and artifact["name"] == NAME and
                artifact["size_in_bytes"] == ZIP_BYTES and artifact["digest"] == "sha256:" + ZIP_SHA and
                artifact["expired"] is False and artifact["workflow_run"]["id"] == RUN and
                artifact["workflow_run"]["head_sha"] == SOURCE, "artifact-metadata")
        stage = "zip-headers"
        reader = HeaderReader(download_location(token))
        report["zipHeaders"] = zip_header_contract(reader)
        report.update(status="headers-verified", freeBytesBeforeDownload=free,
                      authorization="same-repository-read-token",
                      payloadDownloadedByPreflight=False, fullZIPDigestRequiresActionLogGate=True)
    except Exception as error:
        report["error"] = {"stage": stage, "type": type(error).__name__}
        if isinstance(error, urllib.error.HTTPError):
            report["error"]["httpStatus"] = error.code
    args.output.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n")
    return 0 if report["status"] == "headers-verified" else 1


if __name__ == "__main__":
    sys.exit(main())
