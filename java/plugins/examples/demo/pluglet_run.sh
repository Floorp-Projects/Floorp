#!/bin/sh
LD_PRELOAD=libXm.so
export LD_PRELOAD
./mozilla resource:///res/javadev/pluglets/index.html