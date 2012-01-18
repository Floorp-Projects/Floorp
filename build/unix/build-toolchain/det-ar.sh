#!/bin/sh

shift
echo $MOZ_AR "crD" "$@"
exec $MOZ_AR "crD" "$@"
