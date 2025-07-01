# Development Environment Setup

## System Requirements

### Minimum System Requirements

#### Windows
- **OS**: Windows 10 (1903) or later, Windows 11
- **Architecture**: x64, ARM64
- **Memory**: 8GB RAM (16GB recommended)
- **Storage**: 10GB free space
- **Network**: Stable internet connection for downloading Firefox binaries

#### macOS
- **OS**: macOS 10.15 (Catalina) or later
- **Architecture**: Intel x64, Apple Silicon (M1/M2)
- **Memory**: 8GB RAM (16GB recommended)
- **Storage**: 10GB free space
- **Network**: Stable internet connection for downloading Firefox binaries

#### Linux
- **Distribution**: Ubuntu 20.04+, Debian 11+, Fedora 35+, or equivalent
- **Architecture**: x64, ARM64
- **Memory**: 8GB RAM (16GB recommended)
- **Storage**: 10GB free space
- **Network**: Stable internet connection for downloading Firefox binaries

## Required Tools

### 1. Deno (Required)
Deno is the primary runtime for Floorp's build system.

#### Installation
```bash
# Using install script (Windows/macOS/Linux)
curl -fsSL https://deno.land/install.sh | sh

# Windows (using PowerShell)
irm https://deno.land/install.ps1 | iex

# macOS (using Homebrew)
brew install deno

# Linux (using Snap)
sudo snap install deno

# Verify installation
deno --version
```

#### Required Version
- **Minimum**: Deno 1.40.0 or later
- **Recommended**: Latest stable version

### 2. Node.js (Required)
Used for frontend development tools and package management.

#### Installation
```bash
# Download from official website
# https://nodejs.org/

# Using Node Version Manager (recommended)
# Install nvm first
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash

# Install and use Node.js
nvm install 20
nvm use 20

# Verify installation
node --version
npm --version
```

#### Required Version
- **Node.js**: 18.0.0 or later (20.x recommended)
- **npm**: 9.0.0 or later

### 3. Rust (Required for Rust components)
Used for building WebAssembly components and native modules.

#### Installation
```bash
# Install Rust using rustup
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# Add to PATH (restart terminal or run)
source ~/.cargo/env

# Add WebAssembly target
rustup target add wasm32-wasi

# Verify installation
rustc --version
cargo --version
```

#### Required Version
- **Rust**: 1.70.0 or later
- **Cargo**: Included with Rust

### 4. Python (Required for build scripts)
Used by some build scripts and Mozilla's build system.

#### Installation
```bash
# Windows
# Download from https://python.org/

# macOS
brew install python3

# Linux (Ubuntu/Debian)
sudo apt update
sudo apt install python3 python3-pip

# Verify installation
python3 --version
pip3 --version
```

#### Required Version
- **Python**: 3.8.0 or later

### 5. Git (Required)
For version control and patch management.

#### Installation
```bash
# Windows
# Download from https://git-scm.com/

# macOS
brew install git

# Linux (Ubuntu/Debian)
sudo apt install git

# Verify installation
git --version
```

## Step-by-step Setup

### 1. Clone Repository
```bash
# Clone the repository
git clone https://github.com/Floorp-Projects/Floorp.git
cd Floorp

# Or if you have SSH access
git clone git@github.com:Floorp-Projects/Floorp.git
cd Floorp
```

### 2. Install Dependencies
```bash
# Install Node.js dependencies
npm install

# Install Deno dependencies (if needed)
deno cache build.ts

# Verify setup
deno task --help
```

### 3. Initial Build
```bash
# First build (downloads Firefox binaries)
deno task build

# This process may take 10-30 minutes depending on network speed
# Firefox binaries (~200-500MB) will be downloaded and extracted
```

### 4. Start Development Server
```bash
# Start in development mode
deno task dev

# The browser will launch automatically
# Hot reload is enabled for code changes
```

## Development Tools Configuration

### Visual Studio Code

#### Recommended Extensions
```json
{
  "recommendations": [
    "denoland.vscode-deno",
    "rust-lang.rust-analyzer",
    "bradlc.vscode-tailwindcss",
    "ms-vscode.vscode-typescript-next",
    "esbenp.prettier-vscode",
    "dbaeumer.vscode-eslint"
  ]
}
```

#### Settings Configuration
Create `.vscode/settings.json`:
```json
{
  "deno.enable": true,
  "deno.lint": true,
  "deno.unstable": true,
  "typescript.preferences.importModuleSpecifier": "relative",
  "editor.defaultFormatter": "denoland.vscode-deno",
  "[typescript]": {
    "editor.defaultFormatter": "denoland.vscode-deno"
  },
  "[javascript]": {
    "editor.defaultFormatter": "denoland.vscode-deno"
  },
  "[rust]": {
    "editor.defaultFormatter": "rust-lang.rust-analyzer"
  }
}
```

#### Tasks Configuration
Create `.vscode/tasks.json`:
```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Floorp: Dev",
      "type": "shell",
      "command": "deno",
      "args": ["task", "dev"],
      "group": "build",
      "presentation": {
        "echo": true,
        "reveal": "always",
        "focus": false,
        "panel": "shared"
      },
      "problemMatcher": []
    },
    {
      "label": "Floorp: Build",
      "type": "shell",
      "command": "deno",
      "args": ["task", "build"],
      "group": "build",
      "presentation": {
        "echo": true,
        "reveal": "always",
        "focus": false,
        "panel": "shared"
      },
      "problemMatcher": []
    }
  ]
}
```

### Neovim Configuration

#### Using lazy.nvim
```lua
return {
  -- Deno support
  {
    "neovim/nvim-lspconfig",
    opts = {
      servers = {
        denols = {
          root_dir = require("lspconfig.util").root_pattern("deno.json", "deno.jsonc"),
          settings = {
            deno = {
              enable = true,
              lint = true,
              unstable = true,
            },
          },
        },
      },
    },
  },
  
  -- Rust support
  {
    "simrat39/rust-tools.nvim",
    dependencies = "neovim/nvim-lspconfig",
    opts = {},
  },
  
  -- TypeScript support
  {
    "jose-elias-alvarez/typescript.nvim",
    dependencies = "neovim/nvim-lspconfig",
    opts = {},
  },
}
```

### Vim Configuration

#### Basic Configuration
```vim
" Deno support
let g:deno_enable = v:true
let g:deno_lint = v:true
let g:deno_unstable = v:true

" TypeScript/JavaScript settings
autocmd FileType typescript,javascript setlocal expandtab shiftwidth=2 tabstop=2
autocmd FileType rust setlocal expandtab shiftwidth=4 tabstop=4

" Auto format on save
autocmd BufWritePre *.ts,*.js,*.tsx,*.jsx :!deno fmt %
autocmd BufWritePre *.rs :!rustfmt %
```

## Environment Variables

### Required Environment Variables
```bash
# Optional: Custom Firefox binary path
export FIREFOX_PATH="/path/to/custom/firefox"

# Optional: Custom profile directory
export FIREFOX_PROFILE_DIR="/path/to/profile"

# Optional: Development server port
export DEV_SERVER_PORT=3000

# Optional: Enable debug mode
export DEBUG=1
```

### Setting Environment Variables

#### Windows (PowerShell)
```powershell
# Temporary (current session only)
$env:DEBUG = "1"

# Permanent (user-level)
[Environment]::SetEnvironmentVariable("DEBUG", "1", "User")
```

#### macOS/Linux (Bash/Zsh)
```bash
# Add to ~/.bashrc or ~/.zshrc
export DEBUG=1
export DEV_SERVER_PORT=3000

# Reload configuration
source ~/.bashrc  # or source ~/.zshrc
```

## Platform-specific Configuration

### Windows Configuration

#### PowerShell Execution Policy
```powershell
# Allow script execution (run as Administrator)
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

#### Windows Defender Exclusions
Add the following directories to Windows Defender exclusions for better performance:
- Project root directory
- `_dist/` directory
- Node.js installation directory
- Deno installation directory

### macOS Configuration

#### Xcode Command Line Tools
```bash
# Install Xcode Command Line Tools
xcode-select --install
```

#### Gatekeeper Configuration
```bash
# Allow Floorp to run (if needed)
sudo spctl --master-disable
# Re-enable after development
sudo spctl --master-enable
```

### Linux Configuration

#### Required System Packages
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential curl wget git python3 python3-pip

# Fedora
sudo dnf install gcc gcc-c++ make curl wget git python3 python3-pip

# Arch Linux
sudo pacman -S base-devel curl wget git python python-pip
```

#### Firefox Dependencies
```bash
# Ubuntu/Debian
sudo apt install libgtk-3-0 libx11-xcb1 libxcomposite1 libxcursor1 libxdamage1 libxi6 libxtst6 libnss3 libcups2 libxss1 libxrandr2 libasound2 libpangocairo-1.0-0 libatk1.0-0 libcairo-gobject2 libgtk-3-0 libgdk-pixbuf2.0-0

# Fedora
sudo dnf install gtk3 libX11-xcb libXcomposite libXcursor libXdamage libXi libXtst nss cups-libs libXScrnSaver libXrandr alsa-lib pango atk cairo-gobject gdk-pixbuf2
```

## Troubleshooting Common Issues

### Issue: Deno not found
```bash
# Solution: Add Deno to PATH
echo 'export PATH="$HOME/.deno/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### Issue: Permission denied when running scripts
```bash
# Solution: Make scripts executable
chmod +x build.ts
# Or run with explicit permissions
deno run -A build.ts
```

### Issue: Firefox binary download fails
```bash
# Solution: Manual download and extraction
# 1. Check available binaries at: https://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central/
# 2. Download appropriate binary for your platform
# 3. Extract to _dist/bin/ directory
```

### Issue: Build fails with memory error
```bash
# Solution: Increase Node.js memory limit
export NODE_OPTIONS="--max-old-space-size=8192"
deno task build
```

### Issue: Hot reload not working
```bash
# Solution: Check if ports are available
netstat -an | grep :3000
# Kill processes using the port if needed
# Restart development server
deno task dev
```

### Issue: Rust compilation fails
```bash
# Solution: Update Rust toolchain
rustup update stable
rustup target add wasm32-wasi

# Clear Rust cache
cargo clean
deno task build
```

## Performance Optimization

### Build Performance
```bash
# Use multiple CPU cores for compilation
export MAKEFLAGS="-j$(nproc)"  # Linux
export MAKEFLAGS="-j$(sysctl -n hw.ncpu)"  # macOS

# Enable Rust incremental compilation
export CARGO_INCREMENTAL=1
```

### Development Server Performance
```bash
# Increase file watcher limits (Linux)
echo fs.inotify.max_user_watches=524288 | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

## Next Steps

After completing the setup:

1. **Read the Architecture Documentation**: Understand how Floorp is structured
2. **Explore the Codebase**: Start with `src/apps/main/` for the main application
3. **Make Your First Change**: Try modifying a UI component and see hot reload in action
4. **Run Tests**: Execute `deno task test` to run the test suite
5. **Join the Community**: Connect with other developers on Discord or GitHub Discussions

## Getting Help

If you encounter issues during setup:

1. **Check the logs**: Build and development server logs often contain helpful error messages
2. **Search existing issues**: Check GitHub Issues for similar problems
3. **Ask for help**: Post in GitHub Discussions or Discord
4. **Update tools**: Ensure you're using the latest versions of all required tools

The development environment setup is complete once you can successfully run `deno task dev` and see the Floorp browser launch with hot reload functionality.