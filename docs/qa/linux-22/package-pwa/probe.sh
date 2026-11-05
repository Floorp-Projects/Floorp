#!/bin/bash
# Safe capability evidence: never print process environments or credentials.
set +e
date -u +%FT%TZ
cat /etc/os-release
uname -srmo
id
for mode in '-Ur' '-Umr'; do
  printf 'namespace %s: ' "$mode"
  unshare "$mode" true
  printf 'exit=%s\n' "$?"
done
printf 'capabilities and seccomp\n'
sed -n '/^Cap/p;/^Seccomp/p' /proc/self/status
printf 'system bus\n'
ls -l /run/dbus/system_bus_socket
printf 'isolated session bus\n'
dbus-run-session -- dbus-send --session --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.ListNames
printf 'local Docker\n'
env -u DOCKER_HOST -u DOCKER_CONTEXT -u DOCKER_TLS -u DOCKER_TLS_VERIFY -u DOCKER_CERT_PATH docker --host=unix:///var/run/docker.sock info --format '{{.ServerVersion}}'
printf 'screen sockets\n'
ls -l /tmp/.X11-unix /workspace/qa-linux-22/package-pwa/runtime/qa-kde-wayland
printf 'HTTP with default proxy/TLS verification\n'
curl -I --max-time 20 --silent --show-error https://github.com/Floorp-Projects/Floorp
printf 'network policy\n'
cat /etc/codex/network-policy.json
