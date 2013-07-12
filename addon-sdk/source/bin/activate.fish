# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file must be used with "source bin/activate.fish" *from fish*
# you cannot run it directly

# Much of this code is based off of the activate.fish file for the
# virtualenv project. http://ur1.ca/ehmd6

function deactivate -d "Exit addon-sdk and return to normal shell environment"
    if test -n "$_OLD_VIRTUAL_PATH"
        set -gx PATH $_OLD_VIRTUAL_PATH
        set -e _OLD_VIRTUAL_PATH
    end

    if test -n "$_OLD_PYTHONPATH"
        set -gx PYTHONPATH $_OLD_PYTHONPATH
        set -e _OLD_PYTHONPATH
    end

    if test -n "$_OLD_FISH_PROMPT_OVERRIDE"
        functions -e fish_prompt
        set -e _OLD_FISH_PROMPT_OVERRIDE
        . ( begin
                printf "function fish_prompt\n\t#"
                functions _old_fish_prompt
            end | psub )

        functions -e _old_fish_prompt
    end

    set -e CUDDLEFISH_ROOT
    set -e VIRTUAL_ENV

    if test "$argv[1]" != "nondestructive"
        functions -e deactivate
    end
end

# unset irrelavent variables
deactivate nondestructive

set -gx _OLD_PYTHONPATH $PYTHONPATH
set -gx _OLD_VIRTUAL_PATH $PATH
set -gx _OLD_FISH_PROMPT_OVERRIDE "true"

set VIRTUAL_ENV (pwd)

set -gx CUDDLEFISH_ROOT $VIRTUAL_ENV
set -gx PYTHONPATH "$VIRTUAL_ENV/python-lib" $PYTHONPATH
set -gx PATH "$VIRTUAL_ENV/bin" $PATH

# save the current fish_prompt function as the function _old_fish_prompt
. ( begin
        printf "function _old_fish_prompt\n\t#"
        functions fish_prompt
    end | psub )

# with the original prompt function renamed, we can override with our own.
function fish_prompt
    printf "(%s)%s%s" (basename "$VIRTUAL_ENV") (set_color normal) (_old_fish_prompt)
    return
end 

python -c "from jetpack_sdk_env import welcome; welcome()"
