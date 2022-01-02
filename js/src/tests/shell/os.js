// |reftest| skip-if(!xulRuntime.shell||xulRuntime.OS=="WINNT")
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var pid = os.getpid();
assertEq(pid > 0, true);

var PATH = os.getenv("PATH");
assertEq(PATH.indexOf("bin") > 0, true);
assertEq(os.getenv("SQUAMMISH_HILLBILLY_GOAT_SQUEEZERS"), undefined);

assertEq(os.system("true"), 0, "/bin/true should exit 0");
assertEq(os.system("false") != 0, true, "/bin/false should exit nonzero");

var kidpid = os.spawn("sleep 60");
assertEq(kidpid > 0, true, "spawning sleep");
var info = os.waitpid(kidpid, true);
assertEq(info.hasOwnProperty("pid"), false);
assertEq(info.hasOwnProperty("exitStatus"), false);

os.kill(kidpid);

info = os.waitpid(kidpid);
assertEq(info.hasOwnProperty("pid"), true, "waiting on dead process should return pid");
assertEq(info.pid, kidpid);
assertEq(info.hasOwnProperty("exitStatus"), false, "killed process should not have exitStatus");

kidpid = os.spawn("false");
assertEq(kidpid > 0, true, "spawning /bin/false");
info = os.waitpid(kidpid);
assertEq(info.hasOwnProperty("pid"), true, "waiting on dead process should return pid");
assertEq(info.pid, kidpid);
assertEq(info.hasOwnProperty("exitStatus"), true, "process should have exitStatus");
assertEq(info.exitStatus, 1, "/bin/false should exit 1");

reportCompare(true, true);
