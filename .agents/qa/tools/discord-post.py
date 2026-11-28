#!/usr/bin/env python3
"""discord-post.py — post QA reports to Discord via bot token. Zero dependencies.

Reads credentials from (first hit wins):
  1. environment variables DISCORD_BOT_TOKEN / DISCORD_CHANNEL_ID
  2. an env file: --env-file <path>, default .agents/qa/local.env

Usage:
  python3 discord-post.py --message "text" [--file report.md] [--file evidence.jsonl]
                        [--thread-name "name"] [--env-file path] [--dry-run]

Behavior:
  - Messages are chunked to Discord's 2000-char limit.
  - With --thread-name, creates a thread from the first posted message and posts
    remaining chunks + file attachments into the thread (matches the existing
    notify-channel convention).
  - --dry-run prints what would be posted without contacting Discord.
  - Exit code 0 on success, 1 on error, 3 when credentials are not configured.
"""

import json
import os
import sys
import urllib.request
import uuid

DISCORD_API = "https://discord.com/api/v10"
CHUNK = 1900  # safety margin below 2000


def load_env_file(path):
    values = {}
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, v = line.split("=", 1)
                values[k.strip()] = v.strip().strip('"').strip("'")
    except FileNotFoundError:
        pass
    return values


def parse_args(argv):
    opts = {"files": [], "message": None, "thread_name": None, "env_file": None,
            "dry_run": False, "channel": None}
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--message":
            opts["message"] = argv[i + 1]; i += 2
        elif a == "--file":
            opts["files"].append(argv[i + 1]); i += 2
        elif a == "--thread-name":
            opts["thread_name"] = argv[i + 1]; i += 2
        elif a == "--env-file":
            opts["env_file"] = argv[i + 1]; i += 2
        elif a == "--channel":
            opts["channel"] = argv[i + 1]; i += 2
        elif a == "--dry-run":
            opts["dry_run"] = True; i += 1
        else:
            i += 1
    return opts


def api(token, method, path, payload=None, files=None):
    url = DISCORD_API + path
    if files:
        boundary = uuid.uuid4().hex
        body = bytearray()
        if payload is not None:
            body += f"--{boundary}\r\nContent-Disposition: form-data; name=\"payload_json\"\r\nContent-Type: application/json\r\n\r\n".encode()
            body += json.dumps(payload).encode() + b"\r\n"
        for idx, (fname, data) in enumerate(files):
            body += f"--{boundary}\r\nContent-Disposition: form-data; name=\"files[{idx}]\"; filename=\"{fname}\"\r\nContent-Type: application/octet-stream\r\n\r\n".encode()
            body += data + b"\r\n"
        body += f"--{boundary}--\r\n".encode()
        req = urllib.request.Request(
            url, data=bytes(body), method=method,
            headers={
                "Authorization": f"Bot {token}",
                "Content-Type": f"multipart/form-data; boundary={boundary}",
            },
        )
    else:
        req = urllib.request.Request(
            url,
            data=json.dumps(payload).encode() if payload is not None else None,
            method=method,
            headers={
                "Authorization": f"Bot {token}",
                "Content-Type": "application/json",
            },
        )
    with urllib.request.urlopen(req, timeout=30) as res:
        raw = res.read()
        return json.loads(raw) if raw else {}


def chunks(text):
    out = []
    rest = text
    while len(rest) > CHUNK:
        cut = rest.rfind("\n", 0, CHUNK)
        if cut < CHUNK // 2:
            cut = CHUNK
        out.append(rest[:cut])
        rest = rest[cut:].lstrip("\n")
    if rest.strip():
        out.append(rest)
    return out or [""]


def main():
    opts = parse_args(sys.argv[1:])
    script_dir = os.path.dirname(os.path.abspath(__file__))
    default_env = os.path.join(script_dir, "..", "local.env")
    env_file = opts["env_file"] or default_env
    file_env = load_env_file(env_file)

    token = os.environ.get("DISCORD_BOT_TOKEN") or file_env.get("DISCORD_BOT_TOKEN")
    channel = (opts["channel"] or os.environ.get("DISCORD_CHANNEL_ID")
               or file_env.get("DISCORD_CHANNEL_ID"))

    if not opts["message"] and not opts["files"]:
        print("ERROR: nothing to post (--message and/or --file required)", file=sys.stderr)
        return 1

    if opts["dry_run"]:
        print(json.dumps({
            "dry_run": True,
            "channel": channel,
            "credentials": "configured" if token else "missing",
            "message_chunks": len(chunks(opts["message"] or "")),
            "files": opts["files"],
            "thread_name": opts["thread_name"],
        }, ensure_ascii=False))
        return 0

    if not token or not channel:
        print(
            "ERROR: Discord credentials not configured. Set DISCORD_BOT_TOKEN and "
            "DISCORD_CHANNEL_ID in the environment or in .agents/qa/local.env",
            file=sys.stderr,
        )
        return 3

    parts = chunks(opts["message"] or "")
    target_channel = channel
    thread_id = None

    first = api(token, "POST", f"/channels/{target_channel}/messages",
                {"content": parts[0]})
    first_id = first.get("id")

    if opts["thread_name"] and first_id:
        thread = api(token, "POST",
                     f"/channels/{target_channel}/messages/{first_id}/threads",
                     {"name": opts["thread_name"][:100]})
        thread_id = thread.get("id")
        if thread_id:
            target_channel = thread_id

    for part in parts[1:]:
        api(token, "POST", f"/channels/{target_channel}/messages",
            {"content": part})

    for fpath in opts["files"]:
        with open(fpath, "rb") as f:
            data = f.read()
        api(token, "POST", f"/channels/{target_channel}/messages",
            payload={}, files=[(os.path.basename(fpath), data)])

    print(json.dumps({
        "posted": True,
        "channel": channel,
        "thread_id": thread_id,
        "first_message_id": first_id,
        "chunks": len(parts),
        "files": len(opts["files"]),
    }))
    return 0


if __name__ == "__main__":
    sys.exit(main())
