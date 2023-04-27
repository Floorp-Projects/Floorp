import subprocess
from multiprocessing import Pool
import sys

commands_list = [
    [
        r"wget -nv -O - https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.cache.level-1.toolchains.v3.linux64-wine.latest/artifacts/public%2Fbuild%2Fwine.tar.zst | tar --zstd -xv -C $RUNNER_USERDIR/win-crosstool"
    ],
    [
        r"wget -nv -O - https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.cache.level-3.toolchains.v3.linux64-liblowercase.latest/artifacts/public%2Fbuild%2Fliblowercase.tar.zst | tar --zstd -xv -C $RUNNER_USERDIR/win-crosstool"
    ],
    [
        r"wget -nv -O - https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.cache.level-3.toolchains.v3.linux64-rust-windows-1.68.latest/artifacts/public%2Fbuild%2Frustc.tar.zst | tar --zstd -xv -C $RUNNER_USERDIR/win-crosstool"
    ],
    [
        r"sudo apt install -y msitools python3-pip",
        r"pip install six zstandard pyyaml",
        r"cp third_party/python/vsdownload/vsdownload.py vsdownload.py",
        r"cp build/vs/pack_vs.py pack_vs.py",
        r"cp build/vs/vs2017.yaml vs2017.yaml",
        r"python3 pack_vs.py -o $RUNNER_USERDIR/win-crosstool/vs2017.tar.zst vs2017.yaml",
        r"tar --zstd -xvf $RUNNER_USERDIR/win-crosstool/vs2017.tar.zst -C $RUNNER_USERDIR/win-crosstool"
    ]
]

def exec_commands(commands):
    for command in commands:
        if type(command) is str:
            try:
                subprocess.check_call(command, shell=True)
            except Exception:
                print("failed: " + str(command))
                raise Exception
                return
        else:
            try:
                subprocess.check_call(command)
            except Exception:
                print("failed: " + str(command))
                raise Exception
                return

if __name__ == '__main__':
    subprocess.check_call("mkdir $RUNNER_USERDIR/win-crosstool", shell=True)
    processes = []
    pool = Pool(len(commands_list))
    for commands in commands_list:
        process = pool.apply_async(exec_commands, args=(commands, ))
        processes.append({
            "process": process,
            "commands": commands
        })
    pool.close()

    error = False
    error_commands = []
    for process in processes:
        try:
            process["process"].get()
        except Exception:
            error = True
            error_commands.append(process["commands"])
    if error:
        print("failed: " + str(error_commands))
        sys.exit(1)
