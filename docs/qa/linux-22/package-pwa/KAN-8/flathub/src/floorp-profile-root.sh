#!/bin/bash

floorp_select_profile_root() {
  [[ -n ${MOZ_APP_DATA:-} ]] && return 0

  if [[ -z ${XDG_CONFIG_HOME:-} || $XDG_CONFIG_HOME != /*/config ]]; then
    printf '%s\n' 'Floorp: cannot identify the Flatpak app-data directory.' >&2
    return 1
  fi

  local legacy="$XDG_CONFIG_HOME/../.floorp"
  local xdg="$XDG_CONFIG_HOME/floorp"
  local host="${HOME:?}/.floorp"

  if [[ -e "$legacy/profiles.ini" && -e "$xdg/profiles.ini" &&
        ! "$legacy" -ef "$xdg" ]]; then
    printf '%s\n' "Floorp: two Flatpak profile registries exist: $legacy and $xdg. No data was changed; choose which profile to retain before starting." >&2
    return 1
  fi
  if [[ -e "$host" && ! "$host" -ef "$legacy" ]]; then
    printf '%s\n' "Floorp: a separate host profile directory exists at $host. No data was changed; resolve the profile choice before starting." >&2
    return 1
  fi

  local dir entry
  for dir in "$legacy" "$xdg"; do
    if [[ -d "$dir" && ! -e "$dir/profiles.ini" ]]; then
      for entry in "$dir"/* "$dir"/.[!.]* "$dir"/..?*; do
        if [[ -e "$entry" || -L "$entry" ]]; then
          printf '%s\n' "Floorp: profile data without profiles.ini exists at $dir. No data was changed; resolve it before starting." >&2
          return 1
        fi
      done
    fi
  done

  if [[ -e "$xdg/profiles.ini" && ! -e "$legacy/profiles.ini" ]]; then
    export MOZ_APP_DATA="$xdg"
  else
    export MOZ_APP_DATA="$legacy"
  fi
}
