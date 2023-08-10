echo Before:
free -h
df -h

echo
echo

sudo swapoff /mnt/swapfile
sudo rm /mnt/swapfile

sudo fallocate -l 8G /mnt/swapfile
sudo chmod 600 /mnt/swapfile
sudo mkswap /mnt/swapfile
sudo swapon /mnt/swapfile

sudo apt clean
sudo apt remove temurin* openjdk*
sudo apt remove microsoft-edge* firefox* google-chrome*
sudo apt remove mono* libmono* msbuild* dotnet*
sudo apt remove llvm-15* llvm-14* llvm-13* llvm-12* libllvm15* libllvm14* libllvm13* libllvm12*
sudo apt remove gfortran* php* julia* r-*
sudo apt remove mysql* postgresql*
sudo apt remove google-cloud-sdk azure-cli powershell snapd

sudo rm -rf /usr/share/swift &
sudo rm -rf /usr/local/aws-cli &
sudo rm -rf /usr/local/aws-sam-cli &
sudo rm -rf /usr/local/julia* &
sudo rm -rf /usr/local/lib/android &
sudo rm -rf /usr/local/lib/node_modules &
sudo rm -rf /opt/hostedtoolcache &
sudo rm -rf /opt/pipx &
sudo rm -rf /snap &
wait

sudo fallocate -l 12G /home/runner/swapfile
sudo chmod 600 /home/runner/swapfile
sudo mkswap /home/runner/swapfile
sudo swapon /home/runner/swapfile

sudo sysctl vm.swappiness=10

echo
echo

echo After:
free -h
df -h