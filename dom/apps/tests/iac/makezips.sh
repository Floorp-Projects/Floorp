#!/bin/sh

rm publisher/publisher.zip
rm subscriber/subscriber.zip
cd publisher
zip publisher.zip `cat publisher.list`
cd ../subscriber
zip subscriber.zip `cat subscriber.list`
cd ..
