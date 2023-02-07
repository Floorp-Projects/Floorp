import os
import glob
import sys

head_text = [
    "# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-",
    "# vim: set filetype=python:",
    "# This Source Code Form is subject to the terms of the Mozilla Public",
    "# License, v. 2.0. If a copy of the MPL was not distributed with this",
    "# file, You can obtain one at http://mozilla.org/MPL/2.0/.",
    "",
    'DEFINES["MOZ_APP_VERSION"] = CONFIG["MOZ_APP_VERSION"]',
    'DEFINES["MOZ_APP_MAXVERSION"] = CONFIG["MOZ_APP_MAXVERSION"]',
    "",
    ""
]
object_name = "FINAL_TARGET_FILES.features"
extension_id = input("Please type extension id: ")
object_subelement_name = object_name + "[\"" + extension_id + "\"]"

text = "\n".join(head_text) + "\n"

file_list = [p for p in glob.glob("**", recursive=True) if os.path.isfile(p)]
file_dic = {}
file_dic["root"] = []
file_dic["sub"] = {}
for file in file_list:
    if(file == os.path.basename(sys.argv[0]) or file == "moz.build"):
        continue
    splitted = file.replace("\\","/").split("/")
    dir = ""
    for index, name in enumerate(splitted):
        if(index + 1 == len(splitted)):
            pass
        else:
            dir += "[\"" + name + "\"]"
    if(dir == ""):
        file_dic["root"].append(file.replace("\\","/"))
    elif(dir in file_dic["sub"]):
        file_dic["sub"][dir].append(file.replace("\\","/"))
    else:
        file_dic["sub"][dir] = []
        file_dic["sub"][dir].append(file.replace("\\","/"))

text += object_subelement_name + " += [\n"
for file in sorted(file_dic["root"], key=str.lower):
    text += "    \"" + file + "\",\n"
text += "]\n\n"

for path in file_dic["sub"].keys():
    text += object_subelement_name + path + " += [\n"
    for file in sorted(file_dic["sub"][path], key=str.lower):
        text += "    \"" + file + "\",\n"
    text += "]\n\n"

foot_text = [
    'with Files("**"):',
    '    BUG_COMPONENT = ("Firefox", "' + extension_id + '")'
]

text += "\n".join(foot_text) + "\n"

mozbuild = open("moz.build","w")
mozbuild.write(text)
mozbuild.close()
